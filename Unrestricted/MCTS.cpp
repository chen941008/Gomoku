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
/*
Todo list:
1. 調整expansion 拓展範圍，範圍設為方圓一格
2. 調整playout 最大模擬深度至50 ，若超過50則視為平手
3. 調整playout 下棋範圍，為方圓一個後取最大矩形
4. 調整backpropagation 計算方式，加入深度權重
5. 加入審局函式協助判斷
*/
using namespace std;
ThreadPool threadPool(5);
MCTS::MCTS(int simTimes, int numThreads)
    : simulationTimes(simTimes), numThreads(numThreads), generator(std::random_device{}()) {}

int MCTS::run(Node* root, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 1; i <= iterations; i++) {
        if (i % 10000 == 0) {
            cout << "MCTS iteration: " << i << endl << "別急，我在思考中..." << endl;
        }
        Node* selectedNode = selection(root);
        if (selectedNode->isWin) {
            backpropagation(selectedNode, root->parent, selectedNode->isBlackTurn, 1);
            continue;
        }
        if (selectedNode->visits == 0 || selectedNode->children[0] == nullptr) {
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
    // 建立所有已佔據位置的合併位棋盤
    uint64_t combined[BITBOARD_COUNT];
    static constexpr uint64_t LAST_BOARD_MASK = 0xFFFFFFFE00000000;
    for (int i = 0; i < BITBOARD_COUNT; i++) {
        combined[i] = node->boardBlack[i] | node->boardWhite[i];
    }

    // 準備儲存相鄰空位的位棋盤
    uint64_t adjacentEmpty[BITBOARD_COUNT] = {0};

    // 對於每個已佔據的位置，標記其空白的鄰居
    for (int i = 0; i < BITBOARD_COUNT; i++) {
        uint64_t occupied = combined[i];
        if (__builtin_popcountll(occupied) == 0) continue;
        while (occupied) {
            // 跳過你提到的特殊情況
            if (i == 3 && occupied == LAST_BOARD_MASK) {
                break;
            }

            // 找到最低位的 1
            int pos = __builtin_ctzll(occupied);
            int globalPos = pos + i * 64;
            int x = globalLookupTable[globalPos].x;
            int y = globalLookupTable[globalPos].y;

            // 檢查相鄰位置
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    int nx = x + dx;
                    int ny = y + dy;

                    // 跳過超出邊界的座標
                    if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) continue;

                    // 若位置為空，則在 adjacentEmpty 標記
                    if (!getBit(combined, {nx, ny})) {
                        setBit(adjacentEmpty, {nx, ny});
                    }
                }
            }

            // 清除最低位的 1
            occupied &= (occupied - 1);
        }
    }

    // 為每個相鄰空位建立子節點
    int index = 0;
    for (int i = 0; i < BITBOARD_COUNT; i++) {
        uint64_t expandPositions = adjacentEmpty[i];
        while (expandPositions) {
            int pos = __builtin_ctzll(expandPositions);
            int globalPos = pos + i * 64;
            int x = globalLookupTable[globalPos].x;
            int y = globalLookupTable[globalPos].y;

            node->children[index++] = new Node({x, y}, node);

            // 清除最低位的 1
            expandPositions &= (expandPositions - 1);
        }
    }

    // 返回第一個子節點，若無則返回 node
    return index > 0 ? node->children[0] : node;
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
    bool startTurn = node->isBlackTurn;
    bool currentTurn = startTurn;
    thread_local std::mt19937 localRng(std::random_device{}());
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
    const int margin = 1;  // 你可以根据需要调整这个值
    minRow = std::max(minRow - margin, 0);
    maxRow = std::min(maxRow + margin, BOARD_SIZE - 1);
    minCol = std::max(minCol - margin, 0);
    maxCol = std::min(maxCol + margin, BOARD_SIZE - 1);
    for (int i = minRow; i <= maxRow; i++) {
        for (int j = minCol; j <= maxCol; j++) {
            if (!getBit(boardBlack, {i, j}) && !getBit(boardWhite, {i, j})) {
                addPossibleMove(i, j);
            }
        }
    }
    if (moveCount == 0) {
        return 0;
    }
    const int MAX_DEEP = 50;
    for (int step = 0; step < moveCount && step < MAX_DEEP; step++) {
        int randomIndex = step + (localRng() % (moveCount - step));
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
        // 當 move 在 x 軸觸碰邊界時
        if (move.x == minRow && minRow != 0) {
            minRow--;
            for (int col = minCol; col <= maxCol; col++) {
                addPossibleMove(minRow, col);
            }
        } else if (move.x == maxRow && maxRow != BOARD_SIZE - 1) {
            maxRow++;
            for (int col = minCol; col <= maxCol; col++) {
                addPossibleMove(maxRow, col);
            }
        }
        // 當 move 在 y 軸觸碰邊界時
        if (move.y == minCol && minCol != 0) {
            minCol--;
            for (int row = minRow; row <= maxRow; row++) {
                addPossibleMove(row, minCol);
            }
        } else if (move.y == maxCol && maxCol != BOARD_SIZE - 1) {
            maxCol++;
            for (int row = minRow; row <= maxRow; row++) {
                addPossibleMove(row, maxCol);
            }
        }
    }
    return 0;
}
double MCTS::parallelPlayouts(int thread, int simulationTimes, Node* node) {
    assert(thread <= 6 && "Thread count must not exceed 6");
    std::future<int> futures[8];
    int quotient = simulationTimes / thread;
    int remainder = simulationTimes % thread;
    for (int i = 0; i < thread - 1; i++) {  // 最後一個 thread 不用 給主線程執行
        int runTimes = (i < remainder) ? quotient + 1 : quotient;
        // 把每個執行的 future 存到 vector
        futures[i] = threadPool.enqueue([this, runTimes, node]() {
            int results = 0;
            for (int j = 0; j < runTimes; j++) {
                results += this->playout(node);
            }
            return results;
        });
    }
    // 主線程執行
    int totalResults = 0;
    for (int i = 0; i < quotient; i++) {
        totalResults += playout(node);
    }
    for (int i = 0; i < thread - 1; i++) {  // 最後一個 thread 不用 給主線程執行
        totalResults += futures[i].get();
    }
    return static_cast<double>(totalResults) / simulationTimes;
}
