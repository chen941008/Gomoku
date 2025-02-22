#include "MCTS.hpp"

#include <stdint.h>

#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <thread>
#include <vector>

#include "Game.hpp"
#include "Node.hpp"

using namespace std;

MCTS::MCTS(int simTimes) : simulationTimes(simTimes), generator(std::random_device{}()) {}

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
        double playoutResult = 0.0;
        for (int j = 0; j < simulationTimes; j++) {
            playoutResult += playout(selectedNode);
        }
        playoutResult /= simulationTimes;
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

    // 遍历整个棋盘，找出已有棋子的最小和最大行列
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (getBit(node->boardBlack, {i, j}) || getBit(node->boardWhite, {i, j})) {
                minRow = std::min(minRow, i);
                maxRow = std::max(maxRow, i);
                minCol = std::min(minCol, j);
                maxCol = std::max(maxCol, j);
            }
        }
    }

    // 如果棋盘上还没有棋子，则全盘扩展
    if (maxRow == -1) {
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                Node* newNode = new Node({i, j}, node);
                node->children[index++] = newNode;
            }
        }
    } else {
        // 设置一个边界，确保不会超出棋盘范围
        int margin = 2;  // 你可以根据需要调整这个值
        minRow = std::max(minRow - margin, 0);
        maxRow = std::min(maxRow + margin, BOARD_SIZE - 1);
        minCol = std::max(minCol - margin, 0);
        maxCol = std::min(maxCol + margin, BOARD_SIZE - 1);

        // 仅在限定区域内扩展
        for (int i = minRow; i <= maxRow; i++) {
            for (int j = minCol; j <= maxCol; j++) {
                if (getBit(node->boardBlack, {i, j}) || getBit(node->boardWhite, {i, j})) {
                    continue;
                }
                Node* newNode = new Node({i, j}, node);
                node->children[index++] = newNode;
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
    bool startTurn = node->isBlackTurn;
    bool currentTurn = startTurn;
    uint64_t boardBlack[BITBOARD_COUNT];
    uint64_t boardWhite[BITBOARD_COUNT];
    std::copy(std::begin(node->boardBlack), std::end(node->boardBlack), std::begin(boardBlack));
    std::copy(std::begin(node->boardWhite), std::end(node->boardWhite), std::begin(boardWhite));
    Position possibleMoves[MAX_CHILDREN];
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (getBit(boardBlack, {i, j}) || getBit(boardWhite, {i, j})) {
                continue;
            }
            possibleMoves[count++] = {i, j};
        }
    }
    if (count == 0) {
        return 0;
    }
    for (int i = count - 1; i > 0; --i) {
        int j = generator() % (i + 1);
        std::swap(possibleMoves[i], possibleMoves[j]);
    }

    for (int i = count - 1; i >= 0; i--) {
        currentTurn = !currentTurn;
        Position move = possibleMoves[i];

        if (currentTurn) {
            setBit(boardBlack, move);
        } else {
            setBit(boardWhite, move);
        }
        if (Game::checkWin(move, boardBlack, boardWhite, currentTurn)) {
            return (currentTurn == startTurn) ? 1 : -1;
        }
    }
    return 0;
}
