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
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (node->board[i][j] == 0) {
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
    int board[BOARD_SIZE][BOARD_SIZE] = {0};
    Position possibleMoves[MAX_CHILDREN];
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (node->board[i][j] == 0) {
                possibleMoves[count++] = {i, j};
            } else {
                board[i][j] = node->board[i][j];
            }
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
            board[move.x][move.y] = 1;
        } else {
            board[move.x][move.y] = 2;
        }

        if (Game::checkWin(board, move)) {
            return (currentTurn == startTurn) ? 1 : -1;
        }
    }
    return 0;
}
