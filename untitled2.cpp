#include <ctime>
#include <curses.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

// 方块压缩数据 原版7种方块4旋转态二进制编码
const int block[7][4] = {
	{431424, 598356, 431424, 598356},
	{427089, 615696, 427089, 615696},
	{348480, 348480, 348480, 348480},
	{599636, 431376, 598336, 432192},
	{411985, 610832, 415808, 595540},
	{247872, 799248, 247872, 799248},
	{614928, 399424, 615744, 428369}
};

// 游戏面板尺寸
const int BOARD_H = 20;
const int BOARD_W = 10;

class TetrisGame
{
private:
	int board[BOARD_H][BOARD_W]{};
	int x, y;          // 当前方块坐标
	int r;             // 当前旋转
	int px, py, pr;    // 上一帧方块状态
	int p;             // 当前方块类型 0~6
	int tick;
	int score;
	
	// 从压缩数字提取2bit坐标数据
	inline int NUM(int shape, int rot, int shift) const
	{
		return 3 & (block[shape][rot] >> shift);
	}
	
	// 在面板绘制/清除方块
	void set_piece(int bx, int by, int rot, int val)
	{
		for (int i = 0; i < 8; i += 2)
		{
			int dy = NUM(p, rot, i * 2);
			int dx = NUM(p, rot, i * 2 + 2);
			int ty = by + dy;
			int tx = bx + dx;
			// 边界保护 杜绝越界崩溃
			if (ty >= 0 && ty < BOARD_H && tx >=0 && tx < BOARD_W)
				board[ty][tx] = val;
		}
	}
	
	// 碰撞检测：目标位置是否重叠/触底
	bool check_hit(int bx, int by, int rot)
	{
		int maxH = NUM(p, rot, 18);
		if (by + maxH > BOARD_H - 1)
			return true;
		
		// 临时清除旧方块检测
		set_piece(px, py, pr, 0);
		bool hit = false;
		for (int i = 0; i < 8; i += 2)
		{
			int dy = NUM(p, rot, i * 2);
			int dx = NUM(p, rot, i * 2 + 2);
			int ty = by + dy;
			int tx = bx + dx;
			if (ty >=0 && tx >=0 && board[ty][tx] != 0)
			{
				hit = true;
				break;
			}
		}
		// 恢复旧方块
		set_piece(px, py, pr, p + 1);
		return hit;
	}
	
	// 消除满行并计分
	void remove_line()
	{
		int maxH = NUM(p, r, 18);
		int startRow = y;
		int endRow = y + maxH;
		if(endRow >= BOARD_H) endRow = BOARD_H - 1;
		
		for (int row = startRow; row <= endRow; row++)
		{
			bool full = true;
			for (int col = 0; col < BOARD_W; col++)
			{
				if (board[row][col] == 0)
				{
					full = false;
					break;
				}
			}
			if (!full) continue;
			
			// 整行下落
			for (int i = row - 1; i >= 0; i--)
			{
				memcpy(board[i+1], board[i], sizeof(board[i]));
			}
			memset(board[0], 0, sizeof(board[0]));
			score++;
		}
	}
	
	// 生成新方块，游戏顶到边界返回false代表GameOver
	bool new_piece()
	{
		y = py = 0;
		p = std::rand() % 7;
		r = pr = std::rand() % 4;
		int blockW = NUM(p, r, 16);
		int maxStartX = BOARD_W - blockW;
		x = px = std::rand() % maxStartX;
		
		// 生成直接碰撞=游戏结束
		if (check_hit(x, y, r))
			return false;
		update_piece();
		return true;
	}
	
	// 更新方块画面（擦除旧、绘制新）
	void update_piece()
	{
		set_piece(px, py, pr, 0);
		px = x; py = y; pr = r;
		set_piece(px, py, pr, p + 1);
	}
	
	// 渲染游戏面板+分数
	void frame()
	{
		for (int i = 0; i < BOARD_H; i++)
		{
			move(1 + i, 1);
			for (int j = 0; j < BOARD_W; j++)
			{
				int val = board[i][j];
				if (val != 0)
					attron(COLOR_PAIR(val));
				printw("  ");
				if (val != 0)
					attroff(COLOR_PAIR(val));
			}
		}
		move(BOARD_H + 1, 1);
		printw("Score: %d | W旋转 A左 D右 S速降 Q退出", score);
		refresh();
	}
	
	// 方块自动下落计时
	bool do_tick()
	{
		tick++;
		if (tick > 30)
		{
			tick = 0;
			if (check_hit(x, y + 1, r))
			{
				// 触底固化，消行生成新方块
				remove_line();
				return new_piece();
			}
			else
			{
				y++;
				update_piece();
			}
		}
		return true;
	}
	
public:
	TetrisGame() : x(0), y(0), r(0), px(0), py(0), pr(0), p(0), tick(0), score(0)
	{
		memset(board, 0, sizeof(board));
	}
	
	// 游戏主循环
	void runloop()
	{
		if (!new_piece()) return;
		int ch = 0;
		while (do_tick())
		{
			usleep(10000);
			ch = getch();
			int curW = NUM(p, r, 16);
			
			// A 左移
			if (ch == 'a' && x > 0 && !check_hit(x - 1, y, r))
				x--;
			// D 右移
			if (ch == 'd' && x + curW < BOARD_W - 1 && !check_hit(x + 1, y, r))
				x++;
			// S 一键落底
			if (ch == 's')
			{
				while (!check_hit(x, y + 1, r))
				{
					y++;
					update_piece();
				}
				remove_line();
				if (!new_piece()) break;
			}
			// W 旋转
			if (ch == 'w')
			{
				int newR = (r + 1) % 4;
				int newW = NUM(p, newR, 16);
				int tempX = x;
				// 旋转穿墙修正
				while (tempX + newW > BOARD_W - 1) tempX--;
				if (!check_hit(tempX, y, newR))
				{
					r = newR;
					x = tempX;
				}
			}
			// Q 退出
			if (ch == 'q')
				break;
			
			update_piece();
			frame();
		}
		// Game Over提示
		move(BOARD_H / 2, 4);
		printw("GAME OVER! Final Score: %d", score);
		refresh();
		getch();
	}
};

int main()
{
	std::srand(static_cast<unsigned int>(time(nullptr)));
	initscr();
	start_color();
	cbreak();
	noecho();
	timeout(0);
	curs_set(0);
	
	// 初始化7组方块颜色
	for (int i = 1; i <= 7; i++)
		init_pair(i, i, COLOR_BLACK);
	
	resizeterm(24, 32);
	box(stdscr, 0, 0);
	
	TetrisGame game;
	game.runloop();
	
	endwin();
	return 0;
}
