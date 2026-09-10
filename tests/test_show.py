#!/usr/bin/env python3
"""Check the main executable's printed random boards and seeded gameplay."""
import os
from pathlib import Path
import subprocess
import unittest
from test_cli import TerminalGame

ROOT = Path(__file__).resolve().parents[1]
BINARY = str(ROOT / 'minesweeper')


def run_show(rows, columns, mines, first=None, seed=0):
    args = [BINARY, str(rows), str(columns), str(mines), '--show', '--seed', str(seed)]
    if first:
        args += ['--first', f'{first[0]},{first[1]}']
    return subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, text=True,
        env={**os.environ, 'TERM': 'dumb', 'LC_ALL': 'C', 'NO_COLOR': '1'}, timeout=10)


def parse_board(output, rows, columns):
    grid = [line.split() for line in output.splitlines()[-rows:]]
    assert len(grid) == rows and all(len(row) == columns for row in grid)
    flat = sum(grid, [])
    assert all(x in '.*12345678' and len(x) == 1 for x in flat)
    return flat


def audit(flat, rows, columns, total, first):
    mines = {p for p, value in enumerate(flat) if value == '*'}
    assert len(mines) == total and first not in mines
    assert flat[first] == '.', 'first click must be a zero'
    for p, value in enumerate(flat):
        if p in mines:
            continue
        row, col = divmod(p, columns)
        neighbors = {r * columns + c
            for r in range(max(0, row - 1), min(rows, row + 2))
            for c in range(max(0, col - 1), min(columns, col + 2))} - {p}
        assert int(value.replace('.', '0')) == len(neighbors & mines), 'incorrect printed clue'


class ShowTests(unittest.TestCase):
    def test_show_is_plain_and_preserves_counts_and_first_zero(self):
        for rows, columns, total in [(9, 9, 1), (9, 9, 72), (16, 16, 30),
                (20, 20, 120), (20, 20, 200), (20, 20, 300), (20, 20, 391), (30, 31, 450)]:
            for first in [(1, 1), (rows // 2 + 1, columns // 2 + 1), (rows, columns)]:
                with self.subTest(size=(rows, columns), mines=total, first=first):
                    p = run_show(rows, columns, total, first)
                    self.assertEqual(p.returncode, 0, p.stderr)
                    self.assertNotIn('\x1b', p.stdout + p.stderr)
                    audit(parse_board(p.stdout, rows, columns), rows, columns, total,
                        (first[0] - 1) * columns + first[1] - 1)

    def test_seed_reproduction_and_diversity(self):
        layouts = set()
        for seed in (0, 1, 37, 4294967295):
            a = run_show(20, 20, 200, seed=seed)
            b = run_show(20, 20, 200, (11, 11), seed)
            self.assertEqual(a.returncode, 0, a.stderr)
            self.assertEqual(b.returncode, 0, b.stderr)
            board = parse_board(a.stdout, 20, 20)
            self.assertEqual(board, parse_board(b.stdout, 20, 20))
            layouts.add(tuple(board))
        self.assertGreater(len(layouts), 1)

    def test_capacity_depends_on_first_click_and_never_clamps_mine_count(self):
        for first, capacity in [((1, 1), 77), ((1, 5), 75), ((5, 5), 72)]:
            with self.subTest(first=first):
                accepted = run_show(9, 9, capacity, first)
                self.assertEqual(accepted.returncode, 0, accepted.stderr)
                audit(parse_board(accepted.stdout, 9, 9), 9, 9, capacity,
                    (first[0] - 1) * 9 + first[1] - 1)
                rejected = run_show(9, 9, capacity + 1, first)
                self.assertNotEqual(rejected.returncode, 0)
                self.assertIn(f'1..{capacity}', rejected.stderr)
                self.assertEqual(rejected.stdout, '')
                self.assertNotIn('\x1b', rejected.stderr)
        rejected = run_show(9, 9, 0, (1, 1))
        self.assertNotEqual(rejected.returncode, 0)
        self.assertEqual(rejected.stdout, '')

    def test_argument_errors(self):
        for args in [('--first', '0,1'), ('--first', '21,1'), ('--first', '1,1x'),
                     ('--first', '1'), ('--seed', '-1'), ('--seed', '4294967296'),
                     ('--seed', '1e3')]:
            p = subprocess.run([BINARY, '20', '20', '200', '--show', *args],
                stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=10)
            self.assertNotEqual(p.returncode, 0)
            self.assertNotIn('\x1b', p.stdout + p.stderr)
        p = subprocess.run([BINARY, '100000', '100000', '1', '--no-max-size', '--show'],
            stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('INT_MAX', p.stderr)

    def test_show_never_modifies_terminal_settings(self):
        import pty
        import termios
        master, slave = pty.openpty()
        try:
            before = termios.tcgetattr(slave)
            p = subprocess.run([BINARY, '--show', '--seed', '1'], stdin=slave,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertEqual(before, termios.tcgetattr(slave))
            self.assertNotIn(b'\x1b', p.stdout + p.stderr)
        finally:
            os.close(master)
            os.close(slave)

    def test_interactive_board_matches_show(self):
        # Open every safe cell to compare the complete first-click-generated layout.
        for rows, columns, total, first in [(20, 20, 200, (11, 11)),
                (9, 12, 30, (1, 12)), (20, 20, 396, (20, 20))]:
            with self.subTest(size=(rows, columns), first=first):
                printed = run_show(rows, columns, total, first, seed=0)
                self.assertEqual(printed.returncode, 0, printed.stderr)
                expected = parse_board(printed.stdout, rows, columns)
                with TerminalGame([rows, columns, total, '--seed', '0'], rows, columns) as game:
                    # A flagged opening and cursor movement must not generate or
                    # consume RNG before the first successful open at this cell.
                    game.send('f f')
                    self.assertEqual(game.board().count('.'), rows * columns)
                    game.batch([((first[0] - 1) * columns + first[1] - 1, ' ')])
                    for cell, value in enumerate(expected):
                        if value != '*' and game.board()[cell] == '.':
                            game.batch([(cell, ' ')])
                    self.assertIn('you win!', game.text())
                    for cell, value in enumerate(expected):
                        if value != '*':
                            self.assertEqual(int(game.board()[cell].strip() or 0),
                                int(value.replace('.', '0')))


if __name__ == '__main__':
    unittest.main(verbosity=2)
