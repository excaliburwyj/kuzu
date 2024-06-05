#pragma once

#include <utility>

#include "main/kuzu.h"
#include "node_database.h"
#include "node_prepared_statement.h"
#include "node_progress_bar_display.h"
#include "node_query_result.h"
#include <napi.h>

using namespace kuzu::main;
using namespace kuzu::common;

class NodeConnection : public Napi::ObjectWrap<NodeConnection> {
    friend class ConnectionInitAsyncWorker;
    friend class ConnectionExecuteAsyncWorker;
    friend class ConnectionQueryAsyncWorker;
    friend class NodePreparedStatement;

public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    explicit NodeConnection(const Napi::CallbackInfo& info);
    ~NodeConnection() override = default;

private:
    Napi::Value InitAsync(const Napi::CallbackInfo& info);
    void InitCppConnection();
    void SetMaxNumThreadForExec(const Napi::CallbackInfo& info);
    void SetQueryTimeout(const Napi::CallbackInfo& info);
    Napi::Value ExecuteAsync(const Napi::CallbackInfo& info);
    Napi::Value QueryAsync(const Napi::CallbackInfo& info);
    void Close(const Napi::CallbackInfo& info);

private:
    std::shared_ptr<Database> database;
    std::shared_ptr<Connection> connection;
};

class ConnectionInitAsyncWorker : public Napi::AsyncWorker {
public:
    ConnectionInitAsyncWorker(Napi::Function& callback, NodeConnection* nodeConnection)
        : Napi::AsyncWorker(callback), nodeConnection(nodeConnection) {}

    ~ConnectionInitAsyncWorker() override = default;

    void Execute() override {
        try {
            nodeConnection->InitCppConnection();
        } catch (const std::exception& exc) {
            SetError(std::string(exc.what()));
        }
    }

    void OnOK() override { Callback().Call({Env().Null()}); }

    void OnError(Napi::Error const& error) override { Callback().Call({error.Value()}); }

private:
    NodeConnection* nodeConnection;
};

class ConnectionExecuteAsyncWorker : public Napi::AsyncWorker {
public:
    ConnectionExecuteAsyncWorker(Napi::Function& callback, std::shared_ptr<Connection>& connection,
        std::shared_ptr<PreparedStatement> preparedStatement, NodeQueryResult* nodeQueryResult,
        std::unordered_map<std::string, std::unique_ptr<Value>> params,
        Napi::Value progressCallback)
        : Napi::AsyncWorker(callback), connection(connection),
          preparedStatement(std::move(preparedStatement)), nodeQueryResult(nodeQueryResult),
          params(std::move(params)) {
        if (progressCallback.IsFunction()) {
            this->progressCallback = Napi::ThreadSafeFunction::New(Env(),
                progressCallback.As<Napi::Function>(), "ProgressCallback", 0, 1);
        }
    }

    ~ConnectionExecuteAsyncWorker() override = default;

    void Execute() override {
        auto progressBar = connection->getClientContext()->getProgressBar();
        bool trackProgress = progressBar->getProgressBarPrinting();
        if (progressCallback) {
            progressBar->toggleProgressBarPrinting(true);
            progressBar->setDisplay(
                std::make_shared<NodeProgressBarDisplay>(*progressCallback, Env()));
        }
        try {
            auto result =
                connection->executeWithParams(preparedStatement.get(), std::move(params)).release();
            nodeQueryResult->SetQueryResult(result, true);
            if (!result->isSuccess()) {
                SetError(result->getErrorMessage());
            }
        } catch (const std::exception& exc) {
            SetError(std::string(exc.what()));
        }
        if (progressCallback) {
            progressBar->toggleProgressBarPrinting(trackProgress);
            progressBar->setDisplay(ProgressBar::DefaultProgressBarDisplay());
            progressCallback->Release();
        }
    }

    void OnOK() override { Callback().Call({Env().Null()}); }

    void OnError(Napi::Error const& error) override { Callback().Call({error.Value()}); }

private:
    std::shared_ptr<Connection> connection;
    std::shared_ptr<PreparedStatement> preparedStatement;
    NodeQueryResult* nodeQueryResult;
    std::unordered_map<std::string, std::unique_ptr<Value>> params;
    std::optional<Napi::ThreadSafeFunction> progressCallback;
};

class ConnectionQueryAsyncWorker : public Napi::AsyncWorker {
public:
    ConnectionQueryAsyncWorker(Napi::Function& callback, std::shared_ptr<Connection>& connection,
        std::string statement, NodeQueryResult* nodeQueryResult, Napi::Value progressCallback)
        : Napi::AsyncWorker(callback), connection(connection), statement(std::move(statement)),
          nodeQueryResult(nodeQueryResult) {
        if (progressCallback.IsFunction()) {
            this->progressCallback = Napi::ThreadSafeFunction::New(Env(),
                progressCallback.As<Napi::Function>(), "ProgressCallback", 0, 1);
        }
    }

    ~ConnectionQueryAsyncWorker() override = default;

    void Execute() override {
        auto progressBar = connection->getClientContext()->getProgressBar();
        bool trackProgress = progressBar->getProgressBarPrinting();
        if (progressCallback) {
            progressBar->toggleProgressBarPrinting(true);
            progressBar->setDisplay(
                std::make_shared<NodeProgressBarDisplay>(*progressCallback, Env()));
        }
        try {
            auto result = connection->query(statement).release();
            nodeQueryResult->SetQueryResult(result, true);
            if (!result->isSuccess()) {
                SetError(result->getErrorMessage());
            }
        } catch (const std::exception& exc) {
            SetError(std::string(exc.what()));
        }
        if (progressCallback) {
            progressBar->toggleProgressBarPrinting(trackProgress);
            progressBar->setDisplay(ProgressBar::DefaultProgressBarDisplay());
            progressCallback->Release();
        }
    }

    void OnOK() override { Callback().Call({Env().Null()}); }

    void OnError(Napi::Error const& error) override { Callback().Call({error.Value()}); }

private:
    std::shared_ptr<Connection> connection;
    std::string statement;
    NodeQueryResult* nodeQueryResult;
    std::optional<Napi::ThreadSafeFunction> progressCallback;
};
