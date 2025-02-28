#include "MCTS.hpp"

#include <stdint.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <future>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

#include "Game.hpp"
#include "Node.hpp"
#include "ThreadPool.hpp"

using namespace std;
ThreadPool threadPool(8);
MCTS::MCTS(int simTimes, int numThreads)
    : simulationTimes(simTimes), numThreads(numThreads), generator(std::random_device{}()) {}

int MCTS::run(Node* root, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 1; i <= iterations; i++) {
        /*
        if (i % 10000 == 0) {
            cout << "MCTS iteration: " << i << endl << "別急，我在思考中..." << endl;
        }
            */
        Node* selectedNode = selection(root);
        if (selectedNode->isWin) {
            backpropagation(selectedNode, root->parent, selectedNode->isBlackTurn, 1);
            continue;
        }
        if (selectedNode->visits == 0) {
            selectedNode = expansion(selectedNode);
        }
        double playoutResult = parallelPlayouts(numThreads, simulationTimes, selectedNode);
        backpropagation(selectedNode, root->parent, selectedNode->isBlackTurn, playoutResult);
    }
    auto end = std::chrono::high_resolution_clock::now();  // 記錄結束時間
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return duration.count();
}

Node* MCTS::selection(Node* node) {
    while (true) {
        if (node->children[0] == nullptr) {
            return node;
        } else {
            Node* bestChild = nullptr;
            double bestValue = std::numeric_limits<double>::lowest();
            double logParent = log(node->visits);
            for (int i = 0; i < MAX_CHILDREN && node->children[i] != nullptr; i++) {
                Node* child = node->children[i];
                if (child->visits == 0) {
                    return child;
                }
                double ucbValue = (child->wins / child->visits) + COEFFICIENT * sqrt(logParent / child->visits);
                if (ucbValue > bestValue) {
                    bestValue = ucbValue;
                    bestChild = child;
                }
            }
            node = bestChild;
        }
    }
}
Node* MCTS::expansion(Node* node) {
    int index = 0;
    // 初始化边界变量
    int minRow = BOARD_SIZE, maxRow = -1, minCol = BOARD_SIZE, maxCol = -1;
    uint64_t occupied[BITBOARD_COUNT];
    for (int i = 0; i < BITBOARD_COUNT; i++) {
        occupied[i] = node->boardBlack[i] | node->boardWhite[i];
    }
    for (int i = 0; i < BITBOARD_COUNT; i++) {
        uint64_t board = occupied[i];  // 取得 bitboard
        while (board) {
            int bitIndex = __builtin_ctzll(board);  // 找到最右邊的 1 的 index
            int absIndex = i * 64 + bitIndex;       // 計算該 bit 在整個棋盤中的絕對位置
            if (absIndex >= MAX_CHILDREN) {
                break;
            }
            int row = absIndex / BOARD_SIZE;  // 取得 row
            int col = absIndex % BOARD_SIZE;  // 取得 col

            minRow = std::min(minRow, row);
            maxRow = std::max(maxRow, row);
            minCol = std::min(minCol, col);
            maxCol = std::max(maxCol, col);

            board &= board - 1;  // 移除最低位的 1
        }
    }
    // 如果棋盘上还没有棋子，则下在中间
    if (maxRow == -1) {
        Node* newNode = new Node({7, 7}, node);  // 乖乖給老子下中間
        node->children[index++] = newNode;
    } else {
        // 设置一个边界，确保不会超出棋盘范围
        const int margin = 2;  // 你可以根据需要调整这个值
        minRow = std::max(minRow - margin, 0);
        maxRow = std::min(maxRow + margin, BOARD_SIZE - 1);
        minCol = std::max(minCol - margin, 0);
        maxCol = std::min(maxCol + margin, BOARD_SIZE - 1);

        // 仅在限定区域内扩展

        for (int i = 0; i < BITBOARD_COUNT; i++) {
            uint64_t board = ~occupied[i];
            while (board) {
                int bitIndex = __builtin_ctzll(board);  // 找到最右邊的 1 的 index
                int absIndex = i * 64 + bitIndex;       // 計算該 bit 在整個棋盤中的絕對位置
                int row = absIndex / BOARD_SIZE;        // 取得 row
                int col = absIndex % BOARD_SIZE;        // 取得 col
                if (row >= minRow && row <= maxRow && col >= minCol && col <= maxCol) {
                    Node* newNode = new Node({row, col}, node);
                    node->children[index++] = newNode;
                }
                board &= board - 1;  // 移除最低位的 1
            }
        }
    }
    if (index == 0) {
        return node;
    }
    return node->children[generator() % index];
}

void MCTS::backpropagation(Node* node, Node* endNode, bool isXTurn, double win) {
    while (node != endNode) {
        node->visits++;
        if (isXTurn == node->isBlackTurn) {
            node->wins += win;
        } else {
            node->wins -= win;
        }
        node = node->parent;
    }
}

int MCTS::playout(Node* node) {
    static const Position direction[24] = {{-2, -2}, {-2, -1}, {-2, 0}, {-2, 1}, {-2, 2}, {-1, -2}, {-1, -1}, {-1, 0},
                                           {-1, 1},  {-1, 2},  {0, -2}, {0, -1}, {0, 1},  {0, 2},   {1, -2},  {1, -1},
                                           {1, 0},   {1, 1},   {1, 2},  {2, -2}, {2, -1}, {2, 0},   {2, 1},   {2, 2}};
    bool startTurn = node->isBlackTurn;
    bool currentTurn = startTurn;
    int minRow = BOARD_SIZE, maxRow = -1, minCol = BOARD_SIZE, maxCol = -1;
    uint64_t boardBlack[BITBOARD_COUNT];
    uint64_t boardWhite[BITBOARD_COUNT];
    memcpy(boardBlack, node->boardBlack, sizeof(uint64_t) * BITBOARD_COUNT);
    memcpy(boardWhite, node->boardWhite, sizeof(uint64_t) * BITBOARD_COUNT);
    Position possibleMoves[MAX_CHILDREN] = {0};
    int moveCount = 0;
    bool moveUsed[BOARD_SIZE * BOARD_SIZE] = {false};
    auto addPossibleMove = [&](int x, int y) {
        if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) return;
        uint64_t key = x * BOARD_SIZE + y;
        if (!moveUsed[key] && !getBit(boardBlack, {x, y}) && !getBit(boardWhite, {x, y})) {
            possibleMoves[moveCount++] = {x, y};
            moveUsed[key] = true;
        }
    };
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (getBit(boardBlack, {i, j}) || getBit(boardWhite, {i, j})) {
                minRow = std::min(minRow, i);
                maxRow = std::max(maxRow, i);
                minCol = std::min(minCol, j);
                maxCol = std::max(maxCol, j);
            }
        }
    }
    // 设置一个边界，确保不会超出棋盘范围
    int margin = 2;  // 你可以根据需要调整这个值
    minRow = std::max(minRow - margin, 0);
    maxRow = std::min(maxRow + margin, BOARD_SIZE - 1);
    minCol = std::max(minCol - margin, 0);
    maxCol = std::min(maxCol + margin, BOARD_SIZE - 1);
    for (int i = minRow; i <= maxRow; i++) {
        for (int j = minCol; j <= maxCol; j++) {
            if (!getBit(node->boardBlack, {i, j}) && !getBit(node->boardWhite, {i, j})) {
                addPossibleMove(i, j);
            }
        }
    }
    if (moveCount == 0) {
        return 0;
    }
    int lowerBound = 0, upperBound = moveCount;

    for (int step = 0; step < moveCount; step++) {
        int randomIndex = step + (generator() % (moveCount - step));
        std::swap(possibleMoves[step], possibleMoves[randomIndex]);

        currentTurn = !currentTurn;
        Position move = possibleMoves[step];

        // 執行移動
        if (currentTurn) {
            setBit(boardBlack, move);
        } else {
            setBit(boardWhite, move);
        }

        // 檢查是否獲勝
        if (Game::checkWin(move, boardBlack, boardWhite, currentTurn)) {
            return (currentTurn == startTurn) ? 1 : -1;
        }

        // 基於最後一次移動添加新的可能移動
        for (const auto& d : direction) {
            addPossibleMove(move.x + d.x, move.y + d.y);
        }
    }
    return 0;
}
double MCTS::parallelPlayouts(int thread, int simulationTimes, Node* node) {
    assert(thread <= 8 && "Thread count must not exceed 8");
    std::future<int> futures[8];
    int quotient = simulationTimes / thread;
    int remainder = simulationTimes % thread;
    for (int i = 0; i < thread; i++) {  // 修改為 i < thread
        int runTimes = (i < remainder) ? quotient + 1 : quotient;
        // 把每個執行的 future 存到 vector
        futures[i] = threadPool.enqueue([&, runTimes]() {
            int results = 0;
            for (int j = 0; j < runTimes; j++) {
                results += playout(node);
            }
            return results;
        });
    }
    int totalResults = 0;
    for (int i = 0; i < thread; i++) {
        totalResults += futures[i].get();
    }
    return static_cast<double>(totalResults) / simulationTimes;
}