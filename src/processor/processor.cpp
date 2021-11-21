#include "src/processor/include/processor.h"

#include "src/processor/include/physical_plan/operator/aggregate/simple_aggregate.h"
#include "src/processor/include/physical_plan/operator/hash_join/hash_join_build.h"
#include "src/processor/include/physical_plan/operator/hash_join/hash_join_probe.h"
#include "src/processor/include/physical_plan/operator/result_collector.h"
#include "src/processor/include/physical_plan/operator/sink.h"
#include "src/processor/include/physical_plan/result/query_result.h"

using namespace graphflow::common;

namespace graphflow {
namespace processor {

QueryProcessor::QueryProcessor(uint64_t numThreads) {
    for (auto n = 0u; n < numThreads; ++n) {
        threads.emplace_back([&] { run(); });
    }
}

QueryProcessor::~QueryProcessor() {
    stopThreads = true;
    for (auto& thread : threads) {
        thread.join();
    }
}

unique_ptr<QueryResult> QueryProcessor::execute(PhysicalPlan* physicalPlan, uint64_t numThreads) {
    auto lastOperator = physicalPlan->lastOperator.get();
    auto resultCollector = reinterpret_cast<ResultCollector*>(lastOperator);
    // The root pipeline(task) consists of operators and its prevOperator only, because by default,
    // our plan is a linear one. For binary operators, e.g., HashJoin, we always keep probe and its
    // prevOperator in the same pipeline, and decompose build and its prevOperator into another one.
    auto task = make_shared<Task>(resultCollector, numThreads);
    decomposePlanIntoTasks(lastOperator, task.get(), numThreads);
    scheduleTask(task);
    while (!task->isCompleted()) {
        /*busy wait*/
        this_thread::sleep_for(chrono::microseconds(100));
    }
    return move(resultCollector->queryResult);
}

void QueryProcessor::scheduleTask(const shared_ptr<Task>& task) {
    for (auto& dependency : task->children) {
        scheduleTask(dependency);
    }
    while (task->getNumDependenciesFinished() < task->children.size()) {}
    queue.push(task);
}

void QueryProcessor::decomposePlanIntoTasks(
    PhysicalOperator* op, Task* parentTask, uint64_t numThreads) {
    switch (op->operatorType) {
    case HASH_JOIN_PROBE: {
        auto hashJoinProbe = reinterpret_cast<HashJoinProbe*>(op);
        decomposePlanIntoTasks(hashJoinProbe->buildSidePrevOp.get(), parentTask, numThreads);
        decomposePlanIntoTasks(hashJoinProbe->prevOperator.get(), parentTask, numThreads);
    } break;
    case HASH_JOIN_BUILD: {
        auto hashJoinBuild = reinterpret_cast<HashJoinBuild*>(op);
        auto childTask = make_unique<Task>(reinterpret_cast<Sink*>(hashJoinBuild), numThreads);
        decomposePlanIntoTasks(hashJoinBuild->prevOperator.get(), childTask.get(), numThreads);
        parentTask->addChildTask(move(childTask));
    } break;
    case AGGREGATE: {
        auto aggregate = reinterpret_cast<SimpleAggregate*>(op);
        auto childTask = make_unique<Task>(reinterpret_cast<Sink*>(aggregate), numThreads);
        decomposePlanIntoTasks(aggregate->prevOperator.get(), childTask.get(), numThreads);
        parentTask->addChildTask(move(childTask));
    } break;
    case SCAN:
    case LOAD_CSV:
        break;
    default:
        decomposePlanIntoTasks(op->prevOperator.get(), parentTask, numThreads);
        break;
    }
}

void QueryProcessor::run() {
    while (true) {
        if (stopThreads) {
            break;
        }
        auto task = queue.getTask();
        if (!task) {
            this_thread::sleep_for(chrono::microseconds(100));
            continue;
        }
        task->run();
    }
}

} // namespace processor
} // namespace graphflow
