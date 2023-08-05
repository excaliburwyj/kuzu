#include "binder/expression/node_expression.h"
#include "planner/logical_plan/persistent/logical_delete.h"
#include "processor/operator/persistent/delete.h"
#include "processor/plan_mapper.h"

using namespace kuzu::planner;

namespace kuzu {
namespace processor {

static std::unique_ptr<NodeDeleteExecutor> getNodeDeleteExecutor(
    storage::NodesStore* store, const LogicalDeleteNodeInfo& info, const Schema& inSchema) {
    auto node = info.node;
    auto nodeIDPos = DataPos(inSchema.getExpressionPos(*node->getInternalIDProperty()));
    if (node->isMultiLabeled()) {
        std::unordered_map<common::table_id_t, storage::NodeTable*> tableIDToTableMap;
        for (auto tableID : node->getTableIDs()) {
            auto table = store->getNodeTable(tableID);
            tableIDToTableMap.insert({tableID, table});
        }
        return std::make_unique<MultiLabelNodeDeleteExecutor>(
            std::move(tableIDToTableMap), nodeIDPos);
    } else {
        auto table = store->getNodeTable(node->getSingleTableID());
        return std::make_unique<SingleLabelNodeDeleteExecutor>(table, nodeIDPos);
    }
}

std::unique_ptr<PhysicalOperator> PlanMapper::mapDeleteNode(LogicalOperator* logicalOperator) {
    auto logicalDeleteNode = (LogicalDeleteNode*)logicalOperator;
    auto inSchema = logicalDeleteNode->getChild(0)->getSchema();
    auto prevOperator = mapOperator(logicalOperator->getChild(0).get());
    auto nodesStore = &storageManager.getNodesStore();
    std::vector<std::unique_ptr<NodeDeleteExecutor>> executors;
    for (auto& info : logicalDeleteNode->getInfosRef()) {
        executors.push_back(getNodeDeleteExecutor(nodesStore, *info, *inSchema));
    }
    return std::make_unique<DeleteNode>(std::move(executors), std::move(prevOperator),
        getOperatorID(), logicalDeleteNode->getExpressionsForPrinting());
}

static std::unique_ptr<RelDeleteExecutor> getRelDeleteExecutor(
    storage::RelsStore* store, const binder::RelExpression& rel, const Schema& inSchema) {
    auto srcNodePos =
        DataPos(inSchema.getExpressionPos(*rel.getSrcNode()->getInternalIDProperty()));
    auto dstNodePos =
        DataPos(inSchema.getExpressionPos(*rel.getDstNode()->getInternalIDProperty()));
    auto relIDPos = DataPos(inSchema.getExpressionPos(*rel.getInternalIDProperty()));
    auto statistics = &store->getRelsStatistics();
    if (rel.isMultiLabeled()) {
        std::unordered_map<common::table_id_t,
            std::pair<storage::RelTable*, storage::RelsStatistics*>>
            tableIDToTableMap;
        for (auto tableID : rel.getTableIDs()) {
            auto table = store->getRelTable(tableID);
            tableIDToTableMap.insert({tableID, std::make_pair(table, statistics)});
        }
        return std::make_unique<MultiLabelRelDeleteExecutor>(
            std::move(tableIDToTableMap), srcNodePos, dstNodePos, relIDPos);
    } else {
        auto table = store->getRelTable(rel.getSingleTableID());
        return std::make_unique<SingleLabelRelDeleteExecutor>(
            statistics, table, srcNodePos, dstNodePos, relIDPos);
    }
}

std::unique_ptr<PhysicalOperator> PlanMapper::mapDeleteRel(LogicalOperator* logicalOperator) {
    auto logicalDeleteRel = (LogicalDeleteRel*)logicalOperator;
    auto inSchema = logicalDeleteRel->getChild(0)->getSchema();
    auto prevOperator = mapOperator(logicalOperator->getChild(0).get());
    auto relStore = &storageManager.getRelsStore();
    std::vector<std::unique_ptr<RelDeleteExecutor>> Executors;
    for (auto& rel : logicalDeleteRel->getRelsRef()) {
        Executors.push_back(getRelDeleteExecutor(relStore, *rel, *inSchema));
    }
    return std::make_unique<DeleteRel>(std::move(Executors), std::move(prevOperator),
        getOperatorID(), logicalOperator->getExpressionsForPrinting());
}

} // namespace processor
} // namespace kuzu
