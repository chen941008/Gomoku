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
1. 調整expansion 拓展範圍，範圍設為方圓一格 finish
2. 調整playout 最大模擬深度至50 ，若超過50則視為平手 finish
3. 調整playout 下棋範圍，為方圓一個後取最大矩形 finish
4. 調整backpropagation 計算方式，加入深度權重
5. 加入審局函式協助判斷
6. 更換棋盤資料結構，改為uint32_t
*/
using namespace std;
ThreadPool threadPool(6);
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
    // 0. 特殊情況：空棋盤，下在正中間(7, 7)
    if (node->lastMove.x == -1 && node->lastMove.y == -1) {
        node->children[0] = new Node({7, 7}, node);
        return node->children[0];
    }
    // 1. 為每一行建立 15-bit 的 occupancy 掩碼
    //    其中 bit i (0 <= i < BOARD_SIZE) 為 1 表示該行第 i 個位置（最左邊為 i=0，最右邊為 i=BOARD_SIZE-1）已落子。
    uint16_t occupancy[BOARD_SIZE] = {0};
    for (int x = 0; x < BOARD_SIZE; x++) {
        uint32_t rowVal = node->rowBoard[x];
        uint16_t mask = 0;
        for (int col = 0; col < BOARD_SIZE; col++) {
            int shift = ((CELLS_PER_UINT32 - 1) - col) * BIT_PER_CELL;
            uint8_t cell = (rowVal >> shift) & 0b11;
            if (cell != EMPTY) {  // 非 EMPTY 代表已落子（BLACK 或 WHITE）
                mask |= (1 << col);
            }
        }
        occupancy[x] = mask;
    }

    // 2. 為每一行計算候選落子掩碼
    //    鄰近範圍包括同一行左右、以及上一行和下一行相同位置左右。
    uint16_t candidateMask[BOARD_SIZE] = {0};
    for (int x = 0; x < BOARD_SIZE; x++) {
        // 自己這行的鄰近：occupancy[x] 本身，加上向左、向右平移
        uint16_t selfNeighbors = occupancy[x] | (occupancy[x] << 1) | (occupancy[x] >> 1);
        uint16_t aboveNeighbors = 0;
        uint16_t belowNeighbors = 0;
        if (x > 0) {
            aboveNeighbors = occupancy[x - 1] | (occupancy[x - 1] << 1) | (occupancy[x - 1] >> 1);
        }
        if (x < BOARD_SIZE - 1) {
            belowNeighbors = occupancy[x + 1] | (occupancy[x + 1] << 1) | (occupancy[x + 1] >> 1);
        }
        // 合併鄰近（自己行、上行、下行）的掩碼
        uint16_t neighbors = selfNeighbors | aboveNeighbors | belowNeighbors;
        // 排除已落子的位置
        candidateMask[x] = neighbors & ~(occupancy[x]);
    }

    // 3. 根據 candidateMask 為每個候選落子建立子節點
    int index = 0;
    for (int x = 0; x < BOARD_SIZE; x++) {
        uint16_t rowCandidates = candidateMask[x];
        // 逐位檢查候選掩碼中的每個 bit
        for (int y = 0; y < BOARD_SIZE; y++) {
            if (rowCandidates & (1 << y)) {
                // 建立子節點，落子位置為 (x, y)
                node->children[index++] = new Node({x, y}, node);
            }
        }
    }

    return (index > 0) ? node->children[0] : node;
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

    // 建立局部棋盤複本（注意：我們的棋盤是 15 行，每行有 16 格，其中最後一格作為對齊用，真正有效區域為 15*15）
    uint32_t boardRow[BOARD_SIZE];
    uint32_t boardCol[BOARD_SIZE];
    std::copy(std::begin(node->rowBoard), std::end(node->rowBoard), boardRow);
    std::copy(std::begin(node->colBoard), std::end(node->colBoard), boardCol);

    // 初始化落子範圍：我們只考慮 15x15 有效區域
    int minRow = BOARD_SIZE, maxRow = -1, minCol = BOARD_SIZE, maxCol = -1;

    // 掃描棋盤，找出已有落子的位置
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            uint8_t cell = get_piece(i, j, boardRow);
            if (cell == BLACK || cell == WHITE) {
                minRow = std::min(minRow, i);
                maxRow = std::max(maxRow, i);
                minCol = std::min(minCol, j);
                maxCol = std::max(maxCol, j);
            }
        }
    }
    // 若棋盤尚無落子，則預設全部可下
    if (maxRow == -1) {
        minRow = 0;
        maxRow = BOARD_SIZE - 1;
        minCol = 0;
        maxCol = BOARD_SIZE - 1;
    }

    // 設定邊界 margin（確保拓展不超出棋盤）
    const int margin = 1;
    minRow = std::max(minRow - margin, 0);
    maxRow = std::min(maxRow + margin, BOARD_SIZE - 1);
    minCol = std::max(minCol - margin, 0);
    maxCol = std::min(maxCol + margin, BOARD_SIZE - 1);

    // 收集在邊界區域內的所有可能落子點（在 minRow～maxRow 與 minCol～maxCol 範圍內）
    Position possibleMoves[MAX_CHILDREN] = {0};
    int moveCount = 0;
    bool moveUsed[BOARD_SIZE * BOARD_SIZE] = {false};
    auto addPossibleMove = [&](int x, int y) {
        if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) return;
        int key = x * BOARD_SIZE + y;
        if (!moveUsed[key] && (get_piece(x, y, boardRow) == EMPTY)) {
            possibleMoves[moveCount++] = {x, y};
            moveUsed[key] = true;
        }
    };
    for (int i = minRow; i <= maxRow; i++) {
        for (int j = minCol; j <= maxCol; j++) {
            if (get_piece(i, j, boardRow) == EMPTY) {
                addPossibleMove(i, j);
            }
        }
    }

    if (moveCount == 0) return 0;

    const int MAX_DEEP = 50;
    for (int step = 0; step < moveCount && step < MAX_DEEP; step++) {
        // 隨機選擇下一步：隨機洗牌 remaining moves
        int randomIndex = step + (localRng() % (moveCount - step));
        std::swap(possibleMoves[step], possibleMoves[randomIndex]);

        currentTurn = !currentTurn;
        Position move = possibleMoves[step];

        // 執行移動：根據當前輪次落下 BLACK 或 WHITE
        if (currentTurn) {
            set_piece(move.x, move.y, BLACK, boardRow, boardCol);
        } else {
            set_piece(move.x, move.y, WHITE, boardRow, boardCol);
        }

        // 檢查是否獲勝
        if (Game::checkWin(move, boardRow, boardCol, currentTurn)) {
            return (currentTurn == startTurn) ? 1 : -1;
        }

        // 當落子觸及目前邊界時，拓展候選範圍
        // 若落子在 x 軸邊界，則更新該行並嘗試加入新落子點
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
        // 同理，對 y 軸
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
    for (int i = 0; i < thread; i++) {  // 最後一個 thread 不用 給主線程執行
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
    int totalResults = 0;
    for (int i = 0; i < thread - 1; i++) {  // 最後一個 thread 不用 給主線程執行
        totalResults += futures[i].get();
    }
    return static_cast<double>(totalResults) / simulationTimes;
}
