#ifndef GAME_HPP
#define GAME_HPP
#include <stdint.h>
const int BOARD_SIZE = 15;
const int CHECKWIN_THRESHOLD = 4;
struct Node;
struct Position;
enum aiMode { FIXED_SIMULATION_TIMES = 1, VARIABLE_SIMULATION_TIMES = 2 };

class Game {
   private:
    static bool checkDirection(Position lastMove, Position direction, uint64_t* board);
    static void showEachNodeInformation(Node* currentNode);

   public:
    /**
     * @brief 遊戲的主邏輯控制函式
     */
    static void startGame();

    /**
     * @brief 檢查當前棋盤是否有玩家獲勝
     *
     * @param board 棋盤的二維陣列表示
     * @param playTurn 當前玩家的回合 (true 表示 X 玩家, false 表示 O 玩家)
     * @return true 如果當前玩家獲勝
     * @return false 如果當前玩家未獲勝
     */
    static bool checkWin(Position lastMove, uint64_t* boardBlack, uint64_t* boardWhite, bool isBlackTurn);

    /**
     * @brief 輸出棋盤的當前狀態
     *
     * @param board 棋盤的二維陣列表示
     */
    static void printBoard(uint64_t* boardBlack, uint64_t* boardWhite);
    /**
     * @brief 遞迴生成 Tic-Tac-Toe (井字棋) 的完整遊戲樹。
     *
     * 此函式從給定的節點開始，依據當前棋盤狀態，展開所有可能的後續棋步，
     * 直到遊戲結束（勝負已定或棋盤填滿）。
     *
     * @param node 指向當前節點的指標，作為遊戲樹的根節點進行展開。
     *
     * @note 這個函式只適用於井字棋 (Tic-Tac-Toe) 的狀態空間，因為狀態數量較少 (最多 9!)，
     *       若應用於更複雜的遊戲，可能會導致指標過多而導致記憶體不足。
     *
     * @warning 此函式會遞迴建立大量 `Node` 物件，應確保有適當的記憶體管理機制來避免記憶體洩漏。
     */
    // static void generateFullTree(Node* node);
};

#endif  // GAME_HPP
