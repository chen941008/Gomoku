#ifndef NODE_HPP
#define NODE_HPP

#include <stdint.h>

#include <array>
#include <vector>

#include "Game.hpp"
using std::array;
#define WALL 0b00         // 牆壁
#define BLACK 0b01        // 黑子
#define WHITE 0b10        // 白子
#define EMPTY 0b11        // 空位
#define EXTENDED_SIZE 16  // 每行 16 格對齊
#define BIT_PER_CELL 2
#define CELLS_PER_UINT32 (32 / BIT_PER_CELL)  // 每 32-bit 可存 16 格
const int MAX_CHILDREN = 225;                 ///< 每個節點最多的子節點數量（對應 15x15 棋盤）
struct Position {
    int x;
    int y;
};
// 讀取 rowBoard 中指定位置的 cell 值（使用我們之前定義的位移方法）
// 注意：這裡 row_board 為 uint32_t 陣列，BOARD_SIZE 為 15，EXTENDED_SIZE 為 16
inline uint8_t get_piece(int row, int col, const uint32_t* row_board) {
    int row_index = (row * EXTENDED_SIZE + col) / CELLS_PER_UINT32;
    int pos_in_word = (row * EXTENDED_SIZE + col) % CELLS_PER_UINT32;
    int row_shift = ((CELLS_PER_UINT32 - 1) - pos_in_word) * BIT_PER_CELL;
    return (row_board[row_index] >> row_shift) & 0b11;
}
inline uint8_t get_piece(uint32_t board, int index) { return (board >> (((CELLS_PER_UINT32 - 1) - index) * 2)) & 0b11; }

// 設置 rowBoard 中指定位置的 cell 值（使用我們之前定義的位移方法）
// 注意：這裡 row_board 為 uint32_t 陣列，BOARD_SIZE 為 15，EXTENDED_SIZE 為 16
inline void set_piece(int row, int col, uint8_t piece, uint32_t* row_board, uint32_t* col_board) {
    int row_index = (row * EXTENDED_SIZE + col) / CELLS_PER_UINT32;
    int rowPos = (row * EXTENDED_SIZE + col) % CELLS_PER_UINT32;
    int row_shift = ((CELLS_PER_UINT32 - 1) - rowPos) * BIT_PER_CELL;
    row_board[row_index] &= ~(0b11U << row_shift);
    row_board[row_index] |= ((uint32_t)piece << row_shift);

    int col_index = (col * EXTENDED_SIZE + row) / CELLS_PER_UINT32;  // 轉換為列索引
    int colPos = ((col * EXTENDED_SIZE + row) % CELLS_PER_UINT32);
    int col_shift = ((CELLS_PER_UINT32 - 1) - colPos) * BIT_PER_CELL;
    col_board[col_index] &= ~(0b11U << col_shift);
    col_board[col_index] |= ((uint32_t)piece << col_shift);
}
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
    uint32_t rowBoard[BOARD_SIZE];  ///< 棋盤的row狀態
    uint32_t colBoard[BOARD_SIZE];  ///< 棋盤的column狀態
    Node* parent;                   ///< 指向父節點的指標
    Node* children[MAX_CHILDREN];   ///< 指向子節點的指標陣列
    Position lastMove;              ///< 最後一步的位置 (假設 Position 是較小的結構)
    double wins;                    ///< 該節點的獲勝次數
    int visits;                     ///< 該節點的訪問次數
    bool isWin;                     ///< 是否是終局節點
    bool isBlackTurn;

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
        std::fill(rowBoard, rowBoard + BOARD_SIZE, 0xFFFFFFFC);
        std::fill(colBoard, colBoard + BOARD_SIZE, 0xFFFFFFFC);
        // 初始化所有子節點為 nullptr
        memset(children, 0, sizeof(children));
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
        memcpy(rowBoard, parent->rowBoard, sizeof(uint32_t) * BOARD_SIZE);
        memcpy(colBoard, parent->colBoard, sizeof(uint32_t) * BOARD_SIZE);
        // 根據當前玩家，將落子位置標記到對應的棋盤
        set_piece(lastMove.x, lastMove.y, isBlackTurn ? BLACK : WHITE, rowBoard, colBoard);
        // 檢查是否獲勝
        if (Game::checkWin(lastMove, rowBoard, colBoard, isBlackTurn)) {
            isWin = true;
        } else {
            isWin = false;
        }
        // 初始化所有子節點為 nullptr
        memset(children, 0, sizeof(children));
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
