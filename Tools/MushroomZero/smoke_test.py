import random
import subprocess
import sys
from pathlib import Path

H = 10
W = 17


def parse_move(line):
    parts = [int(x) for x in line.strip().split()]
    if len(parts) != 4:
        raise AssertionError(f"bad move line: {line!r}")
    return parts


def valid(board, move):
    r1, c1, r2, c2 = move
    if move == [-1, -1, -1, -1]:
        return not any_valid(board)
    if not (0 <= r1 <= r2 < H and 0 <= c1 <= c2 < W):
        return False
    total = 0
    top = bottom = left = right = False
    for r in range(r1, r2 + 1):
        for c in range(c1, c2 + 1):
            if board[r][c] != 0:
                total += board[r][c]
                top |= r == r1
                bottom |= r == r2
                left |= c == c1
                right |= c == c2
    return total == 10 and top and bottom and left and right


def any_valid(board):
    for r1 in range(H):
        for r2 in range(r1, H):
            for c1 in range(W):
                for c2 in range(c1, W):
                    if valid(board, [r1, c1, r2, c2]):
                        return True
    return False


def apply(board, move):
    if move == [-1, -1, -1, -1]:
        return
    r1, c1, r2, c2 = move
    for r in range(r1, r2 + 1):
        for c in range(c1, c2 + 1):
            board[r][c] = 0


def random_board(seed):
    rng = random.Random(seed)
    return [[rng.randint(1, 9) for _ in range(W)] for _ in range(H)]


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: smoke_test.py <solver.exe>")

    exe = Path(sys.argv[1])
    board = random_board(7)
    proc = subprocess.Popen(
        [str(exe)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    def send(line):
        proc.stdin.write(line + "\n")
        proc.stdin.flush()

    def recv():
        line = proc.stdout.readline()
        if not line:
            raise AssertionError("solver closed stdout")
        return line.strip()

    send("READY FIRST")
    assert recv() == "OK"
    send("INIT " + " ".join("".join(map(str, row)) for row in board))

    passes = 0
    for _ in range(120):
        send("TIME 10000 10000")
        move = parse_move(recv())
        assert valid(board, move), f"invalid move {move}"
        if move == [-1, -1, -1, -1]:
            passes += 1
            if passes >= 2:
                break
        else:
            passes = 0
            apply(board, move)
        send("OPP -1 -1 -1 -1 0")
        passes += 1
        if passes >= 2:
            break

    send("FINISH")
    proc.wait(timeout=3)
    print("smoke ok")


if __name__ == "__main__":
    main()
