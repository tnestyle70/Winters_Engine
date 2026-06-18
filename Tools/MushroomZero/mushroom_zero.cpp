#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

namespace
{
constexpr int H = 10;
constexpr int W = 17;
constexpr int N = H * W;
constexpr int INF = 1000000000;

struct Move
{
    int r1 = -1;
    int c1 = -1;
    int r2 = -1;
    int c2 = -1;
};

struct State
{
    array<unsigned char, N> board{};
    array<signed char, N> owner{};
    int scoreDiff = 0; // my cells - opponent cells
    int sumLeft = 0;
};

struct Prefix
{
    int sum[H + 1][W + 1]{};
    int rowNz[H][W + 1]{};
    int colNz[W][H + 1]{};
};

struct Timeout
{
};

inline int idx(int r, int c)
{
    return r * W + c;
}

inline bool isPass(const Move& m)
{
    return m.r1 < 0;
}

int areaOf(const Move& m)
{
    return (m.r2 - m.r1 + 1) * (m.c2 - m.c1 + 1);
}

void buildPrefix(const State& s, Prefix& p)
{
    memset(&p, 0, sizeof(p));
    for (int r = 0; r < H; ++r)
    {
        for (int c = 0; c < W; ++c)
        {
            int v = s.board[idx(r, c)];
            p.sum[r + 1][c + 1] = p.sum[r][c + 1] + p.sum[r + 1][c] - p.sum[r][c] + v;
            p.rowNz[r][c + 1] = p.rowNz[r][c] + (v != 0);
            p.colNz[c][r + 1] = p.colNz[c][r] + (v != 0);
        }
    }
}

inline int rectSum(const Prefix& p, int r1, int c1, int r2, int c2)
{
    return p.sum[r2 + 1][c2 + 1] - p.sum[r1][c2 + 1] - p.sum[r2 + 1][c1] + p.sum[r1][c1];
}

inline bool hasEdgeMushroom(const Prefix& p, int r1, int c1, int r2, int c2)
{
    const bool top = p.rowNz[r1][c2 + 1] - p.rowNz[r1][c1] > 0;
    const bool bottom = p.rowNz[r2][c2 + 1] - p.rowNz[r2][c1] > 0;
    const bool left = p.colNz[c1][r2 + 1] - p.colNz[c1][r1] > 0;
    const bool right = p.colNz[c2][r2 + 1] - p.colNz[c2][r1] > 0;
    return top && bottom && left && right;
}

vector<Move> generateMoves(const State& s)
{
    Prefix p;
    buildPrefix(s, p);

    vector<Move> moves;
    moves.reserve(512);

    for (int r1 = 0; r1 < H; ++r1)
    {
        for (int r2 = r1; r2 < H; ++r2)
        {
            for (int c1 = 0; c1 < W; ++c1)
            {
                for (int c2 = c1; c2 < W; ++c2)
                {
                    if (rectSum(p, r1, c1, r2, c2) == 10 && hasEdgeMushroom(p, r1, c1, r2, c2))
                    {
                        moves.push_back({r1, c1, r2, c2});
                    }
                }
            }
        }
    }

    return moves;
}

int deltaForSide(const State& s, const Move& m, int side)
{
    if (isPass(m))
    {
        return 0;
    }

    int delta = 0;
    for (int r = m.r1; r <= m.r2; ++r)
    {
        for (int c = m.c1; c <= m.c2; ++c)
        {
            const int o = s.owner[idx(r, c)];
            if (side == 1)
            {
                if (o == 0)
                    delta += 1;
                else if (o == -1)
                    delta += 2;
            }
            else
            {
                if (o == 0)
                    delta -= 1;
                else if (o == 1)
                    delta -= 2;
            }
        }
    }
    return delta;
}

int gainForSide(const State& s, const Move& m, int side)
{
    const int d = deltaForSide(s, m, side);
    return side == 1 ? d : -d;
}

State applyMove(const State& s, const Move& m, int side)
{
    State t = s;
    if (isPass(m))
    {
        return t;
    }

    for (int r = m.r1; r <= m.r2; ++r)
    {
        for (int c = m.c1; c <= m.c2; ++c)
        {
            const int i = idx(r, c);
            const int oldOwner = t.owner[i];
            if (side == 1)
            {
                if (oldOwner == 0)
                    t.scoreDiff += 1;
                else if (oldOwner == -1)
                    t.scoreDiff += 2;
                t.owner[i] = 1;
            }
            else
            {
                if (oldOwner == 0)
                    t.scoreDiff -= 1;
                else if (oldOwner == 1)
                    t.scoreDiff -= 2;
                t.owner[i] = -1;
            }

            if (t.board[i] != 0)
            {
                t.sumLeft -= t.board[i];
                t.board[i] = 0;
            }
        }
    }

    return t;
}

int countNonZeroInMove(const State& s, const Move& m)
{
    int count = 0;
    for (int r = m.r1; r <= m.r2; ++r)
    {
        for (int c = m.c1; c <= m.c2; ++c)
        {
            count += s.board[idx(r, c)] != 0;
        }
    }
    return count;
}

int candidateScore(const State& s, const Move& m, int side)
{
    const int gain = gainForSide(s, m, side);
    const int area = areaOf(m);
    const int nonZero = countNonZeroInMove(s, m);

    int steal = 0;
    int own = 0;
    for (int r = m.r1; r <= m.r2; ++r)
    {
        for (int c = m.c1; c <= m.c2; ++c)
        {
            const int o = s.owner[idx(r, c)];
            if (o == -side)
                ++steal;
            else if (o == side)
                ++own;
        }
    }

    return gain * 1000 + steal * 180 + area * 8 + nonZero * 12 - own * 10;
}

void sortAndTrimMoves(const State& s, vector<Move>& moves, int side, int cap)
{
    struct ScoredMove
    {
        Move move;
        int score = 0;
        int area = 0;
    };

    vector<ScoredMove> scored;
    scored.reserve(moves.size());
    for (const Move& move : moves)
    {
        scored.push_back({move, candidateScore(s, move, side), areaOf(move)});
    }

    sort(scored.begin(), scored.end(), [](const ScoredMove& a, const ScoredMove& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        if (a.area != b.area)
            return a.area > b.area;
        if (a.move.r1 != b.move.r1)
            return a.move.r1 < b.move.r1;
        return a.move.c1 < b.move.c1;
    });

    if (static_cast<int>(moves.size()) > cap)
    {
        moves.resize(cap);
    }
    for (int i = 0; i < static_cast<int>(moves.size()); ++i)
    {
        moves[i] = scored[i].move;
    }
}

void addTop(vector<int>& best, int value, int limit)
{
    best.push_back(value);
    for (int i = static_cast<int>(best.size()) - 1; i > 0 && best[i] > best[i - 1]; --i)
    {
        swap(best[i], best[i - 1]);
    }
    if (static_cast<int>(best.size()) > limit)
    {
        best.pop_back();
    }
}

int staticEval(const State& s)
{
    vector<Move> moves = generateMoves(s);
    vector<int> myTop;
    vector<int> oppTop;
    myTop.reserve(4);
    oppTop.reserve(4);

    for (const Move& m : moves)
    {
        addTop(myTop, gainForSide(s, m, 1), 4);
        addTop(oppTop, gainForSide(s, m, -1), 4);
    }

    int myPotential = 0;
    int oppPotential = 0;
    for (int i = 0; i < static_cast<int>(myTop.size()); ++i)
    {
        myPotential += myTop[i] * (5 - i);
    }
    for (int i = 0; i < static_cast<int>(oppTop.size()); ++i)
    {
        oppPotential += oppTop[i] * (5 - i);
    }

    return s.scoreDiff * 10000 + (myPotential - oppPotential) * 80;
}

class Searcher
{
public:
    Move choose(const State& root, int myTimeMs)
    {
        vector<Move> moves = generateMoves(root);
        if (moves.empty())
        {
            return {-1, -1, -1, -1};
        }

        const int budgetMs = chooseBudget(root, myTimeMs);
        deadline = chrono::steady_clock::now() + chrono::milliseconds(budgetMs);
        nodes = 0;

        sortAndTrimMoves(root, moves, 1, rootCap(budgetMs, static_cast<int>(moves.size())));

        Move bestMove = moves.front();
        int bestScore = -INF;
        const int probeCount = min(static_cast<int>(moves.size()), fallbackProbeCap(budgetMs));
        for (int i = 0; i < probeCount; ++i)
        {
            if ((i & 7) == 0 && chrono::steady_clock::now() >= deadline)
                break;
            const Move& m = moves[i];
            const State next = applyMove(root, m, 1);
            const int reply = bestReplyGain(next, -1);
            const int score = staticEval(next) - reply * 650;
            if (score > bestScore)
            {
                bestScore = score;
                bestMove = m;
            }
        }

        Move completedBest = bestMove;
        for (int depth = 1; depth <= maxDepthForBudget(budgetMs); ++depth)
        {
            try
            {
                int depthBestScore = -INF;
                Move depthBestMove = completedBest;
                for (const Move& m : moves)
                {
                    if (timedOut())
                        throw Timeout{};
                    const State next = applyMove(root, m, 1);
                    const int value = minimax(next, depth - 1, -1, -INF, INF, false);
                    if (value > depthBestScore)
                    {
                        depthBestScore = value;
                        depthBestMove = m;
                    }
                }
                completedBest = depthBestMove;
            }
            catch (const Timeout&)
            {
                break;
            }
        }

        return completedBest;
    }

private:
    chrono::steady_clock::time_point deadline{};
    int nodes = 0;

    bool timedOut()
    {
        if ((++nodes & 1023) != 0)
            return false;
        return chrono::steady_clock::now() >= deadline;
    }

    int minimax(const State& s, int depth, int side, int alpha, int beta, bool prevPass)
    {
        if (timedOut())
            throw Timeout{};

        if (depth <= 0)
        {
            return staticEval(s);
        }

        vector<Move> moves = generateMoves(s);
        if (moves.empty())
        {
            if (prevPass)
                return s.scoreDiff * 100000;
            return minimax(s, depth - 1, -side, alpha, beta, true);
        }

        sortAndTrimMoves(s, moves, side, capForDepth(depth));

        if (side == 1)
        {
            int best = -INF;
            for (const Move& m : moves)
            {
                const int value = minimax(applyMove(s, m, side), depth - 1, -side, alpha, beta, false);
                best = max(best, value);
                alpha = max(alpha, best);
                if (alpha >= beta)
                    break;
            }
            return best;
        }

        int best = INF;
        for (const Move& m : moves)
        {
            const int value = minimax(applyMove(s, m, side), depth - 1, -side, alpha, beta, false);
            best = min(best, value);
            beta = min(beta, best);
            if (alpha >= beta)
                break;
        }
        return best;
    }

    int bestReplyGain(const State& s, int side)
    {
        vector<Move> moves = generateMoves(s);
        int best = 0;
        for (const Move& m : moves)
        {
            best = max(best, gainForSide(s, m, side));
        }
        return best;
    }

    int chooseBudget(const State& s, int myTimeMs) const
    {
        if (myTimeMs <= 0)
            return 80;

        const int expectedMyTurns = max(6, s.sumLeft / 20 + 3);
        int budget = myTimeMs / expectedMyTurns;
        if (myTimeMs < 500)
            budget = min(budget, 25);
        else if (myTimeMs < 1500)
            budget = min(budget, 60);
        else if (myTimeMs < 5000)
            budget = min(budget, 120);
        else
            budget = min(budget, 260);
        return max(12, budget);
    }

    int rootCap(int budgetMs, int moveCount) const
    {
        int cap = 220;
        if (budgetMs < 40)
            cap = 90;
        else if (budgetMs < 90)
            cap = 140;
        else if (budgetMs < 170)
            cap = 180;
        return min(cap, moveCount);
    }

    int capForDepth(int depth) const
    {
        if (depth >= 5)
            return 18;
        if (depth == 4)
            return 26;
        if (depth == 3)
            return 38;
        if (depth == 2)
            return 58;
        return 90;
    }

    int maxDepthForBudget(int budgetMs) const
    {
        if (budgetMs < 30)
            return 2;
        if (budgetMs < 80)
            return 3;
        if (budgetMs < 170)
            return 4;
        return 5;
    }

    int fallbackProbeCap(int budgetMs) const
    {
        if (budgetMs < 35)
            return 32;
        if (budgetMs < 80)
            return 64;
        if (budgetMs < 160)
            return 120;
        return 180;
    }
};

class Game
{
public:
    void ready(bool isFirst)
    {
        first = isFirst;
        lastActionPass = false;
        state = State{};
    }

    void init(const vector<string>& rows)
    {
        state = State{};
        for (int r = 0; r < H; ++r)
        {
            for (int c = 0; c < W; ++c)
            {
                const unsigned char v = static_cast<unsigned char>(rows[r][c] - '0');
                state.board[idx(r, c)] = v;
                state.sumLeft += v;
            }
        }
        lastActionPass = false;
    }

    Move calculateMove(int myTime, int oppTime)
    {
        (void)oppTime;
        Searcher searcher;
        Move move = searcher.choose(state, myTime);
        updateMove(move, true);
        return move;
    }

    void updateOpponentAction(const Move& action, int time)
    {
        (void)time;
        updateMove(action, false);
    }

private:
    State state{};
    bool first = true;
    bool lastActionPass = false;

    void updateMove(const Move& action, bool mine)
    {
        if (isPass(action))
        {
            lastActionPass = true;
            return;
        }

        state = applyMove(state, action, mine ? 1 : -1);
        lastActionPass = false;
    }
};
} // namespace

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Game game;
    string command;

    while (cin >> command)
    {
        if (command == "READY")
        {
            string order;
            cin >> order;
            game.ready(order == "FIRST");
            cout << "OK" << endl;
        }
        else if (command == "INIT")
        {
            vector<string> rows(H);
            for (int i = 0; i < H; ++i)
                cin >> rows[i];
            game.init(rows);
        }
        else if (command == "TIME")
        {
            int myTime = 0;
            int oppTime = 0;
            cin >> myTime >> oppTime;
            Move move = game.calculateMove(myTime, oppTime);
            cout << move.r1 << ' ' << move.c1 << ' ' << move.r2 << ' ' << move.c2 << endl;
        }
        else if (command == "OPP")
        {
            Move action;
            int time = 0;
            cin >> action.r1 >> action.c1 >> action.r2 >> action.c2 >> time;
            game.updateOpponentAction(action, time);
        }
        else if (command == "FINISH")
        {
            break;
        }
    }

    return 0;
}
