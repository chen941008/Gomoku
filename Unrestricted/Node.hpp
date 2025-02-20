#ifndef NODE_HPP
#define NODE_HPP

#include <stdint.h>

#include <array>
#include <vector>
using std::array;

const int MAX_CHILDREN = 225;  ///< 每個節點最多的子節點數量（對應 15x15 棋盤）
struct Position {
    int x;
    int y;
};

/**
 * @brief 表示遊戲節點的結構體，用於蒙特卡洛樹搜索 (MCTS)
 *
 * 這個結構體代表遊戲樹中的每個節點，包含棋盤狀態、勝利次數、訪問次數等資訊。
 * 每個節點可以有多個子節點，並且每個節點有對應的父節點，這些節點構成了整個蒙特卡洛樹。
 * 節點使用位棋盤 (bitboard) 來表示當前的棋盤狀態，其中：
 * - `boardX` 記錄 X 玩家的落子位置
 * - `boardO` 記錄 O 玩家的落子位置
 * - `isXTurn` 記錄當前是否輪到 X 玩家
 */
struct Node {
    double wins;                        ///< 該節點的獲勝次數
    int visits;                         ///< 該節點的訪問次數
    int board[BOARD_SIZE][BOARD_SIZE];  ///< 棋盤狀態
    bool isBlackTurn;                   ///< 是否是 Black玩家的回合
    Node* parent;                       ///< 指向父節點的指標
    Node* children[MAX_CHILDREN];       ///< 指向子節點的指標陣列
    Position lastMove;                  ///< 最後一步的位置
    bool isWin;                         ///< 是否是終局節點

    /**
     * @brief 預設構造函數，初始化根節點
     *
     * 該構造函數用於初始化根節點：
     * - 勝利次數 (`wins`) 設為 0
     * - 訪問次數 (`visits`) 設為 0
     * - 棋盤狀態 (`board`) 設為 0（表示棋盤為空）
     * - `isBlackTurn` 設為 `false`（假設 Black 先手）
     * - 父節點 (`parent`) 設為 `nullptr`
     * - 所有子節點 (`children`) 初始化為空指標
     */
    Node() : wins(0), visits(0), parent(nullptr), isBlackTurn(false), lastMove({-1, -1}), isWin(false) {
        // 初始化棋盤為全 0 (空棋盤)
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                board[i][j] = 0;
            }
        }

        // 初始化所有子節點為 nullptr
        std::fill(std::begin(children), std::end(children), nullptr);
    }

    /**
     * @brief 構造函數，用於創建子節點
     *
     * 這個構造函數基於父節點創建子節點：
     * - `move` 表示該子節點對應的棋盤移動位置
     * - 新節點會繼承父節點的棋盤狀態
     * - `isBlackTurn` 會與父節點相反，表示輪流落子
     * - 根據 `isBlackTurn`，將 `move` 位置標記到對應的棋盤
     *
     * @param move 該節點對應的棋盤移動位置 (0~8)，表示當前玩家落子的格子
     * @param parent 指向父節點的指標，表示該子節點由哪個父節點衍生
     */
    Node(Position lastMove, Node* parent)
        : wins(0), visits(0), parent(parent), isBlackTurn(!parent->isBlackTurn), lastMove(lastMove) {
        // 繼承父節點的棋盤狀態
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                board[i][j] = parent->board[i][j];
            }
        }

        // 根據當前玩家的回合更新棋盤
        if (isBlackTurn) {
            // 假設對應的棋盤位置已被更新
            board[lastMove.x][lastMove.y] = 1;  // 這裡假設 Black 使用 1
        } else {
            board[lastMove.x][lastMove.y] = 2;  // 這裡假設 White 使用 2
        }
        if (Game::checkWin(board, lastMove)) {
            isWin = true;
        } else {
            isWin = false;
        }
        // 初始化所有子節點為 nullptr
        std::fill(std::begin(children), std::end(children), nullptr);
    }
};

/**
 * @brief 刪除整棵樹，釋放所有節點的記憶體
 *
 * @param node 根節點
 */
inline void deleteTree(Node* node) {
    if (node == nullptr) return;

    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (node->children[i] != nullptr) {
            deleteTree(node->children[i]);
            node->children[i] = nullptr;  // 防止重複刪除
        }
    }

    delete node;
}

#endif  // NODE_HPP
