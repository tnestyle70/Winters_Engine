#include "FX/Graph/FxGraphValidator.h"

#include <algorithm>
#include <sstream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
    void AddIssue(FxValidationResult& result, u32_t nodeId, const char* message)
    {
        FxValidationIssue issue{};
        issue.nodeId = nodeId;
        issue.message = message;
        issue.bError = true;
        result.issues.push_back(std::move(issue));
    }

    void AddIssue(FxValidationResult& result, u32_t nodeId, std::string message)
    {
        FxValidationIssue issue{};
        issue.nodeId = nodeId;
        issue.message = std::move(message);
        issue.bError = true;
        result.issues.push_back(std::move(issue));
    }

    u32_t StageRank(eFxNodeStage stage)
    {
        switch (stage)
        {
        case eFxNodeStage::Spawn: return 0;
        case eFxNodeStage::Init: return 1;
        case eFxNodeStage::Update: return 2;
        case eFxNodeStage::Render: return 3;
        default: return 0;
        }
    }

    std::string FormatEdgeIssue(const char* prefix, u32_t from, u32_t to)
    {
        std::ostringstream oss;
        oss << prefix << " from=" << from << " to=" << to;
        return oss.str();
    }
}

FxValidationResult CFxGraphValidator::Validate(const FxEmitterGraph& emitter)
{
    FxValidationResult result{};

    std::unordered_map<u32_t, size_t> nodeIndexById;
    nodeIndexById.reserve(emitter.nodes.size());

    u32_t renderNodeCount = 0;
    for (size_t i = 0; i < emitter.nodes.size(); ++i)
    {
        const FxGraphNode& node = emitter.nodes[i];
        if (node.id == 0)
            AddIssue(result, node.id, "node_id_zero");

        if (!nodeIndexById.emplace(node.id, i).second)
            AddIssue(result, node.id, "duplicate_node_id");

        if (FxIsRenderNode(node.type))
            ++renderNodeCount;
    }

    if (renderNodeCount == 0)
        AddIssue(result, 0, "missing_render_node");
    else if (renderNodeCount > 1)
        AddIssue(result, 0, "multiple_render_nodes");

    std::unordered_map<u32_t, std::vector<u32_t>> adjacency;
    std::unordered_map<u32_t, u32_t> indegree;
    adjacency.reserve(emitter.nodes.size());
    indegree.reserve(emitter.nodes.size());

    for (const FxGraphNode& node : emitter.nodes)
        indegree[node.id] = 0;

    for (const FxGraphEdge& edge : emitter.edges)
    {
        const bool_t bHasFrom = nodeIndexById.find(edge.from) != nodeIndexById.end();
        const bool_t bHasTo = nodeIndexById.find(edge.to) != nodeIndexById.end();
        if (!bHasFrom || !bHasTo)
        {
            AddIssue(result, bHasFrom ? edge.to : edge.from,
                FormatEdgeIssue("edge_endpoint_missing", edge.from, edge.to));
            continue;
        }

        const FxGraphNode& fromNode = emitter.nodes[nodeIndexById[edge.from]];
        const FxGraphNode& toNode = emitter.nodes[nodeIndexById[edge.to]];
        const u32_t fromStage = StageRank(FxStageFromNodeType(fromNode.type));
        const u32_t toStage = StageRank(FxStageFromNodeType(toNode.type));
        if (fromStage > toStage)
        {
            AddIssue(result, edge.from,
                FormatEdgeIssue("stage_order_regression", edge.from, edge.to));
        }

        adjacency[edge.from].push_back(edge.to);
        ++indegree[edge.to];
    }

    std::queue<u32_t> ready;
    for (const FxGraphNode& node : emitter.nodes)
    {
        if (indegree[node.id] == 0)
            ready.push(node.id);
    }

    while (!ready.empty())
    {
        const u32_t nodeId = ready.front();
        ready.pop();
        result.topoOrder.push_back(nodeId);

        const auto it = adjacency.find(nodeId);
        if (it == adjacency.end())
            continue;

        for (const u32_t nextId : it->second)
        {
            u32_t& nextIndegree = indegree[nextId];
            if (nextIndegree == 0)
                continue;

            --nextIndegree;
            if (nextIndegree == 0)
                ready.push(nextId);
        }
    }

    if (result.topoOrder.size() != nodeIndexById.size())
        AddIssue(result, 0, "graph_cycle_detected");

    result.bValid = std::none_of(result.issues.begin(), result.issues.end(),
        [](const FxValidationIssue& issue)
        {
            return issue.bError;
        });

    if (!result.bValid)
        result.topoOrder.clear();

    return result;
}
