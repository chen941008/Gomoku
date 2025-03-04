#include "Game.hpp"

#include <stdint.h>

#include <iomanip>
#include <iostream>

#include "MCTS.hpp"
#include "Node.hpp"

// Function declaration for expansion

using namespace std;
void Game::startGame() {
    Node* root = new Node();
    // generateFullTree(root);
    Node* currentNode = root;  // CurrentNode為當前棋盤最後一個子的節點，會去選擇他的子節點來下棋
    int playerOrder, currentOrder = 0, aiMode, iterationTimes, simulationTimes;
    cout << "Input stimulation times." << endl;
    cin >> simulationTimes;
    MCTS ai(simulationTimes, 5);
    ai.expansion(currentNode);
    cout << "Choose AI simulation mode: 1 = fixed simulation times, 2 = "
            "variable simulation times"
         << endl;
    while (true) {
        cin >> aiMode;
        if (aiMode == aiMode::FIXED_SIMULATION_TIMES) {
            while (true) {
                cout << "Input how many iteration you want to run (must be "
                        "greater than 225)."
                     << endl;
                cin >> iterationTimes;
                if (iterationTimes > MAX_CHILDREN) {
                    break;
                }
                cout << "Please input a number greater than 225." << endl;
            }
            break;
        }
        if (aiMode == aiMode::VARIABLE_SIMULATION_TIMES) {
            break;
        }
        cout << "Please input 1 or 2" << endl;
    }
    // 用 bitboard 表示棋盤，初始皆為 0
    uint32_t rowBoard[BOARD_SIZE] = {0};
    uint32_t colBoard[BOARD_SIZE] = {0};
    std::fill(rowBoard, rowBoard + BOARD_SIZE, 0xFFFFFFFC);
    std::fill(colBoard, colBoard + BOARD_SIZE, 0xFFFFFFFC);
    cout << "Choose first or second player, input 1 or 2" << endl;
    while (true) {  // 選擇先手或後手，防白痴crash程式
        cin >> playerOrder;
        if (playerOrder == 1 || playerOrder == 2) {
            break;
        }
        cout << "Please input 1 or 2" << endl;
    }
    playerOrder--;
    while (true) {
        if (currentOrder == MAX_CHILDREN) {
            cout << "Draw" << endl;
            printBoard(rowBoard);
            break;
        }
        printBoard(rowBoard);
        if (currentOrder % 2 == playerOrder) {  // Player turn
            showEachNodeInformation(currentNode);
            cout << "Your turn" << endl;
            int X, Y;
            cout << "input X Y 0~14" << endl;
            while (true) {
                cin >> X >> Y;
                if (X < 0 || X > (BOARD_SIZE - 1) || Y < 0 || Y > (BOARD_SIZE - 1)) {
                    cout << "Please input 0~14" << endl;
                    continue;
                }
                // 檢查該位置是否已被佔用
                if (get_piece(X, Y, rowBoard) != EMPTY) {
                    cout << "This position is already taken" << endl;
                    continue;
                }
                break;
            }
            set_piece(X, Y, currentOrder % 2 == 0 ? BLACK : WHITE, rowBoard, colBoard);
            if (currentOrder >= CHECKWIN_THRESHOLD && checkWin({X, Y}, rowBoard, colBoard, currentOrder % 2 == 0)) {
                cout << "You win" << endl;
                printBoard(rowBoard);
                break;
            }
            bool found = false;
            for (int i = 0; i < MAX_CHILDREN && currentNode->children[i] != nullptr; i++) {
                if (currentNode->children[i]->lastMove.x == X && currentNode->children[i]->lastMove.y == Y) {
                    currentNode = currentNode->children[i];
                    found = true;
                    break;
                }
            }
            // 如果玩家的落子不在拓展的節點中，則新建一個節點並加入
            if (!found) {
                Node* newNode = new Node({X, Y}, currentNode);
                currentNode->children[0] = newNode;
                currentNode = newNode;
            }
        } else {  // AI turn
            cout << "AI turn" << endl;
            if (aiMode == aiMode::VARIABLE_SIMULATION_TIMES) {
                cout << "Input how many iteration you want to run (must be "
                        "greater than 225)."
                     << endl;
                do {
                    cin >> iterationTimes;
                    if (iterationTimes <= MAX_CHILDREN) {
                        cout << "Please input a number greater than 225." << endl;
                    }
                } while (iterationTimes <= MAX_CHILDREN);
            }
            if (currentOrder == 0) {
                Node* newNode = new Node({7, 7}, currentNode);
                currentNode->children[0] = newNode;
                currentNode = newNode;
                set_piece(7, 7, BLACK, rowBoard, colBoard);
                cout << "AI choose " << "7" << " " << "7" << endl;
                currentOrder++;
                continue;
            }
            ai.run(currentNode, iterationTimes);
            Node* bestChild = nullptr;
            int mostVisit = 0;
            for (int i = 0; i < MAX_CHILDREN && currentNode->children[i] != nullptr; ++i) {
                Node* child = currentNode->children[i];
                if (child->visits > mostVisit) {
                    mostVisit = child->visits;
                    bestChild = child;
                }
            }
            Position lastMove = bestChild->lastMove;
            set_piece(lastMove.x, lastMove.y, currentOrder % 2 == 0 ? BLACK : WHITE, rowBoard, colBoard);
            showEachNodeInformation(currentNode);
            cout << "AI choose " << lastMove.x << " " << lastMove.y << endl;
            currentNode = bestChild;
            if (currentOrder >= CHECKWIN_THRESHOLD && checkWin(lastMove, rowBoard, colBoard, currentOrder % 2 == 0)) {
                cout << "AI win" << endl;
                printBoard(rowBoard);
                break;
            }
        }
        currentOrder++;
        Node* parent = currentNode->parent;
        for (int i = 0; i < MAX_CHILDREN && parent->children[i] != nullptr; i++) {
            if (parent->children[i] == currentNode) {
                continue;
            }
            deleteTree(parent->children[i]);
        }
    }
    delete root;
}

void Game::printBoard(uint32_t rowBoard[BOARD_SIZE]) {
    cout << endl;

    // 印出上方的欄位標題（0 ~ BOARD_SIZE-1）
    cout << "    ";  // 左上角空白區域
    for (int j = 0; j < BOARD_SIZE; j++) {
        // 為了對齊，假設 j 為單一數字時多留兩個空格，兩位數則一個空格
        if (j < 10)
            cout << j << "   ";
        else
            cout << j << "  ";
    }
    cout << endl;

    // 印出上方的分隔線
    cout << "   ";
    for (int j = 0; j < BOARD_SIZE; j++) {
        cout << "----";
    }
    cout << endl;

    // 印出每一列
    for (int i = 0; i < BOARD_SIZE; i++) {
        // 印出行號（左邊標題）
        if (i < 10)
            cout << i << "  |";
        else
            cout << i << " |";

        // 印出該行的每個棋子
        for (int j = 0; j < BOARD_SIZE; j++) {
            // 取得 rowBoard[i] 中第 j 格的 2-bit 值
            // 我們定義右邊 (j 越大) 對應較低位元，因此 get_piece() 定義為：
            // inline uint8_t get_piece(uint32_t board, int index) {
            //     return (board >> (((CELLS_PER_UINT32 - 1) - index) * BIT_PER_CELL)) & 0b11;
            // }
            uint8_t cell = get_piece(rowBoard[i], j);
            if (cell == BLACK) {
                cout << " X ";
            } else if (cell == WHITE) {
                cout << " O ";
            } else {
                cout << "   ";
            }
            if (j < BOARD_SIZE - 1) {
                cout << "|";
            }
        }
        cout << endl;

        // 每行後面加上分隔線（除了最後一行）
        if (i < BOARD_SIZE - 1) {
            cout << "   ";  // 與行號對齊的空白
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (j == 0) {
                    cout << "-";
                }
                cout << "---";
                if (j < BOARD_SIZE - 1) {
                    cout << "+";
                }
            }
            cout << endl;
        }
    }
    // 印出下方的分隔線
    cout << "   ";
    for (int j = 0; j < BOARD_SIZE; j++) {
        cout << "----";
    }
    cout << endl;
}

/*
// 遞迴生成完整遊戲樹的函式
void Game::generateFullTree(Node* node) {
    // 取得當前棋盤已使用的位置
    uint16_t usedPositions = node->boardX | node->boardO;

    // 如果棋盤已滿，或者當前局面已經分出勝負，就不再展開
    if (usedPositions == 0b111111111) {
        node->state = BoardState::DRAW;
        return;
    } else if (checkWin(node->boardX, node->boardO, node->isXTurn)) {
        node->state = BoardState::WIN;
        return;
    }

    int childIndex = 0;
    // 對於棋盤上的每個位置，若未被使用，則產生一個新節點
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (!(usedPositions & (1 << i))) {      // 若位置 i 未被使用
            Node* newNode = new Node(i, node);  // 依據父節點狀態及這步 move 建立新節點
            node->children[childIndex++] = newNode;
        }
    }

    // 遞迴產生每個子節點的遊戲樹
    for (int i = 0; i < childIndex; i++) {
        generateFullTree(node->children[i]);
    }
}
*/
inline uint32_t Game::packageBits(uint32_t board, int startIndex, int endIndex) {
    uint32_t result = 0;
    for (int i = startIndex; i <= endIndex; i++) {
        result = (result << 2) | get_piece(board, i);
    }
    return result;
}
inline bool Game::isFiveConsecutive(uint32_t board) {
    // 使用 10 位全 1 的掩码
    const uint16_t windowMask = 0x3FF;  // 0b1111111111
    for (int i = 0; i < 5; i++) {
        uint16_t windowPattern = board & windowMask;
        if ((windowPattern == BLACK_WIN) || (windowPattern == WHITE_WIN)) {
            return true;
        }
        board >>= 2;  // 滑動窗口：移動一格
    }
    return false;
}
// 總體檢查是否勝利
bool Game::checkWin(Position lastMove, uint32_t rowBoard[BOARD_SIZE], uint32_t colBoard[BOARD_SIZE], bool isBlackTurn) {
    int x = lastMove.x, y = lastMove.y;
    uint32_t rowPackaged = 0;
    uint32_t colPackaged = 0;
    // 檢查水平方向
    if (y < 4) {
        rowPackaged = packageBits(rowBoard[x], 0, y + 4);
    } else if (y > 10) {
        rowPackaged = packageBits(rowBoard[x], y - 4, 14);
    } else {
        rowPackaged = packageBits(rowBoard[x], y - 4, y + 4);
    }
    if (isFiveConsecutive(rowPackaged)) {
        return true;
    }
    // 檢查垂直方向
    if (x < 4) {
        colPackaged = packageBits(colBoard[y], 0, x + 4);
    } else if (x > 10) {
        colPackaged = packageBits(colBoard[y], x - 4, 14);
    } else {
        colPackaged = packageBits(colBoard[y], x - 4, x + 4);
    }
    if (isFiveConsecutive(colPackaged)) {
        return true;
    }
    // --- 主对角线检查（左上到右下） ---
    // 以 lastMove 为中心，向左上和右下各延伸最多 4 格（受边界限制）
    int d1 = std::min({4, x, y});                                    // 向左上延伸的格数
    int d2 = std::min({4, BOARD_SIZE - 1 - x, BOARD_SIZE - 1 - y});  // 向右下延伸的格数
    uint32_t diagPackaged = 0;
    // 遍历区间从 -d1 到 +d2，共 (d1 + d2 + 1) 个 cell
    for (int i = -d1; i <= d2; i++) {
        int r = x + i;
        int c = y + i;
        // 这里使用 rowBoard[r] 和列索引 c（根据 get_piece 的实现）
        uint8_t cell = get_piece(rowBoard[r], c);
        diagPackaged = (diagPackaged << 2) | cell;
    }
    if (isFiveConsecutive(diagPackaged)) return true;

    // --- 反对角线检查（右上到左下） ---
    // 以 lastMove 为中心，向右上和左下各延伸最多 4 格
    int a1 = std::min({4, x, BOARD_SIZE - 1 - y});  // 向右上延伸的格数
    int a2 = std::min({4, BOARD_SIZE - 1 - x, y});  // 向左下延伸的格数
    uint32_t antiDiagPackaged = 0;
    for (int i = -a1; i <= a2; i++) {
        int r = x + i;
        int c = y - i;
        uint8_t cell = get_piece(rowBoard[r], c);
        antiDiagPackaged = (antiDiagPackaged << 2) | cell;
    }
    if (isFiveConsecutive(antiDiagPackaged)) return true;
    return false;
}
void Game::showEachNodeInformation(Node* currentNode) {
    for (int i = 0; i < MAX_CHILDREN && currentNode->children[i] != nullptr; i++) {
        // 設定固定格式與寬度
        std::cout << std::fixed << std::setprecision(3) << "move: " << std::setw(3)
                  << currentNode->children[i]->lastMove.x << " " << std::setw(3) << currentNode->children[i]->lastMove.y
                  << " | wins: " << std::setw(8) << currentNode->children[i]->wins << " | visits: " << std::setw(8)
                  << currentNode->children[i]->visits << " | winRate: " << std::setw(8)
                  << (currentNode->children[i]->wins / currentNode->children[i]->visits) << std::endl;
    }
}