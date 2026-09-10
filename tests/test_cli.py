#!/usr/bin/env python3
"""PTY integration tests for gameplay and terminal controls."""
import fcntl
import os
from pathlib import Path
import pty
import re
import select
import signal
import struct
import subprocess
import termios
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = str(ROOT / 'minesweeper')


class TerminalGame:
    def __init__(self, args=(), rows=20, columns=20):
        self.rows, self.columns = rows, columns
        self.master, self.slave = pty.openpty()
        self.original = termios.tcgetattr(self.slave)
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack('HHHH', 32, 100, 0, 0))
        self.process = subprocess.Popen([BINARY, *map(str, args)], stdin=self.slave,
            stdout=self.slave, stderr=self.slave,
            env={**os.environ, 'TERM': 'xterm', 'LC_ALL': 'C', 'NO_COLOR': '1'})
        self.screen = [[' '] * 120 for _ in range(45)]
        self.raw_output = ""
        self.row = self.column = self.cursor = 0
        self.read_until(lambda: 'rest square:' in self.text())

    def read(self):
        chunks = []
        while select.select([self.master], [], [], 0.015)[0]:
            try:
                chunk = os.read(self.master, 1000000)
            except OSError:
                break
            if not chunk:
                break
            chunks.append(chunk)
        raw = b''.join(chunks).decode('utf-8', 'replace')
        self.raw_output += raw
        for token in re.split(r'(\x1b\[[0-9;?]*[A-Za-z])', raw):
            if token.startswith('\x1b['):
                args, command = token[2:-1], token[-1]
                if command in 'Hf':
                    nums = [int(n or 1) for n in args.split(';')]
                    self.row = max(0, nums[0] - 1)
                    self.column = max(0, (nums[1] if len(nums) > 1 else 1) - 1)
                elif command == 'J' and args == '2':
                    self.screen = [[' '] * 120 for _ in range(45)]
                elif command == 'K' and self.row < 45:
                    for col in range(self.column, 120):
                        self.screen[self.row][col] = ' '
            else:
                for char in token:
                    if char == '\r':
                        self.column = 0
                    elif char == '\n':
                        self.row += 1
                    elif 0 <= self.row < 45 and 0 <= self.column < 120:
                        self.screen[self.row][self.column] = char
                        self.column += 1
        for row in range(self.rows):
            for col in range(self.columns):
                if self.screen[row + 1][col * 3 + 1] == '[':
                    self.cursor = row * self.columns + col
        return raw

    def read_until(self, predicate, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.read()
            if predicate():
                return
            if self.process.poll() is not None:
                break
        raise AssertionError('TUI did not reach expected state:\n' + self.text())

    def text(self):
        return '\n'.join(''.join(row).rstrip() for row in self.screen)

    def board(self):
        return [self.screen[row + 1][col * 3 + 2]
            for row in range(self.rows) for col in range(self.columns)]

    def send(self, keys):
        os.write(self.master, keys.encode())
        raw = self.read()
        return raw

    def batch(self, actions):
        keys = ''
        current = self.cursor
        for cell, key in actions:
            row, col = divmod(current, self.columns)
            target_row, target_col = divmod(cell, self.columns)
            keys += ('s' * (target_row - row) if target_row >= row else 'w' * (row - target_row))
            keys += ('d' * (target_col - col) if target_col >= col else 'a' * (col - target_col))
            keys += key
            current = cell
        return self.send(keys)

    def mouse(self, button, column, row, release=False):
        return self.send(f'\x1b[<{button};{column};{row}' + ('m' if release else 'M'))

    def resize(self, rows, columns):
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, columns, 0, 0))
        os.kill(self.process.pid, signal.SIGWINCH)
        return self.read()

    def close(self):
        try:
            if self.process.poll() is None:
                self.send('q')
                self.process.wait(timeout=3)
            self.read()
            if '\x1b[?1000h' in self.raw_output:
                assert '\x1b[?1000l' in self.raw_output
                assert '\x1b[?1006l' in self.raw_output
            restored = termios.tcgetattr(self.slave)
            # macOS may set its kernel PENDIN bookkeeping bit on tcsetattr.
            restored[3] &= ~getattr(termios, 'PENDIN', 0)
            expected = self.original.copy()
            expected[3] &= ~getattr(termios, 'PENDIN', 0)
            assert restored == expected, 'Terminal settings were not restored'
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()
            os.close(self.master)
            os.close(self.slave)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class CliTests(unittest.TestCase):
    def test_cli_validation_and_help(self):
        help_result = subprocess.run([BINARY, '--help'], capture_output=True, text=True)
        self.assertEqual(help_result.returncode, 0)
        self.assertIn('--show', help_result.stdout)
        self.assertIn('--mode', help_result.stdout)
        self.assertNotIn('--min-hard', help_result.stdout)
        self.assertEqual(subprocess.run([BINARY, '--version'], capture_output=True).returncode, 0)
        for args in [('--mode', 'connected'), ('--mode=grammar',),
                     ('--mode=unknown',), ('--min-hard', '0'), ('--unknown',),
                     ('--no-max-size', '100000', '100000', '10', '--show')]:
            with self.subTest(args=args):
                result = subprocess.run([BINARY, *args], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('\x1b[?25l', result.stdout)

    def test_presets_and_unlimited_viewport(self):
        for flag, size in [('-E', 9), ('-N', 16), ('-H', 20)]:
            with TerminalGame([flag], size, size) as game:
                self.assertEqual(game.board().count('.'), size * size)
        with TerminalGame(['--no-max-size', '101', '9', '100'], 20, 9) as game:
            self.assertIn('size       :101x9', game.text())
            self.assertIn('warning:clipped', game.text())

    def test_panel_alignment(self):
        with TerminalGame([20, 20, 200, "--seed", "0"]) as game:
            panel_rows = [7, 8, 9, 10, 14, 15, 16, 17]
            colons = [game.screen[row].index(':') for row in panel_rows]
            self.assertEqual(len(set(colons)), 1)
            for row in panel_rows:
                self.assertNotEqual(game.screen[row][63], ' ')
            # All four title-box right edges share the same column.
            self.assertEqual([game.screen[row][79] for row in [1, 2, 3, 4]], ['+', '|', '|', '+'])
            game.batch([(210, ' ')])
            self.assertIn(game.board()[210], ' 012345678')
            first_board = game.board()
            game.resize(19, 55)
            self.assertIn('warning:clipped', game.text())
            game.resize(32, 100)
            self.assertEqual(game.board(), first_board)
            game.send('r')
            game.batch([(210, ' ')])
            self.assertIn(game.board()[210], ' 012345678')
            # Capture the real rendered terminal for visual review.
            if os.environ.get('MINESWEEPER_CAPTURE'):
                Path(os.environ['MINESWEEPER_CAPTURE']).write_text(game.text())
    def test_mouse_clicks_and_ignored_events(self):
        with TerminalGame([20, 20, 200, "--seed", "0"]) as game:
            self.assertIn('\x1b[?1000h', game.raw_output)
            self.assertIn('\x1b[?1006h', game.raw_output)
            # Right click the padding, rather than the center character.
            game.mouse(2, 32, 12)
            self.assertEqual(game.board()[210], '@')
            game.mouse(2, 32, 12, release=True)
            game.mouse(0, 33, 12)
            self.assertEqual(game.board()[210], '@')
            for button, col, row in [(2, 80, 12), (0, 62, 4), (64, 33, 12), (32, 33, 12)]:
                game.mouse(button, col, row)
            self.assertEqual(game.board().count('@'), 1)
            self.assertEqual(game.board().count('.'), 399)
            game.mouse(2, 34, 12)
            game.mouse(0, 33, 12)
            self.assertIn(game.board()[210], ' 012345678')
            before = game.board()
            game.mouse(0, 33, 12, release=True)
            game.mouse(2, 33, 12)
            self.assertEqual(game.board(), before)
            # Fragmented report; no part is interpreted as a key.
            game.send('\x1b[<2;')
            game.send('3;2M')
            self.assertEqual(game.board()[0], '@')
            raw = game.send('\x1b[C')
            self.assertNotIn('\x1b[2J', raw)
            self.assertEqual(game.cursor, 1)

    def test_mouse_border_panning_and_resize_mapping(self):
        with TerminalGame([20, 20, 200, "--seed", "0"]) as game:
            game.resize(19, 55)  # 14 rows by 10 columns, border at x=32,y=16.
            raw = game.mouse(0, 32, 8)
            self.assertNotIn('\x1b[2J', raw)
            self.assertIn('<', ''.join(game.screen[1]))
            self.assertIn('rest square:400', game.text())
            # Visible top left now maps to logical column 2.
            game.mouse(2, 3, 2)
            game.mouse(0, 10, 16)
            self.assertIn('rest square:399', game.text())
            # Visible top left now maps to logical row 2, column 2.
            game.mouse(2, 3, 2)
            game.mouse(0, 10, 1)
            game.mouse(0, 1, 8)
            game.resize(32, 100)
            self.assertEqual(game.board()[1], '@')
            self.assertEqual(game.board()[21], '@')
            self.assertEqual(game.board().count('@'), 2)
            # At a fully visible boundary left clicks cannot alter the board.
            before = game.board()
            game.mouse(0, 1, 1)
            self.assertEqual(game.board(), before)
            game.resize(12, 30)
            game.mouse(0, 3, 2)
            game.mouse(2, 3, 2)
            game.resize(32, 100)
            self.assertEqual(game.board(), before)
            game.resize(19, 55)
            # Clamp at the bottom-right corner; each click can pan both axes.
            for _ in range(25):
                game.mouse(0, 32, 16)
            game.mouse(2, 30, 15)
            game.resize(32, 100)
            self.assertEqual(game.board()[399], '@')
            self.assertEqual(game.board().count('@'), 3)

    def test_flag_before_first_open_and_restart(self):
        with TerminalGame([20, 20, 389, "--seed", "0"]) as game:
            game.send('f ')
            self.assertEqual(game.board()[0], '@')
            self.assertEqual(game.board().count('.'), 399)
            game.send('f')
            game.batch([(210, ' ')])
            self.assertIn(game.board()[210], ' 012345678')
            before = game.board()
            raw = game.send('\x1b[C')
            self.assertNotIn('\x1b[2J', raw)
            self.assertEqual(game.cursor, 211)
            self.assertEqual(before, game.board())
            game.send('r')
            self.assertEqual(game.board().count('.'), 400)
            game.batch([(399, ' ')])
            self.assertIn(game.board()[399], ' 012345678')

    def test_resize_does_not_regenerate(self):
        with TerminalGame([20, 20, 389, "--seed", "0"]) as game:
            game.send(' ')
            before = game.board()
            game.resize(19, 55)
            self.assertIn('warning:clipped', game.text())
            game.resize(32, 100)
            self.assertEqual(game.board(), before)
            game.resize(12, 30)
            self.assertIn('terminal too small', game.text())
            game.resize(32, 100)
            self.assertEqual(game.board(), before)

    def test_restart_and_quit_after_opening(self):
        with TerminalGame([20, 20, 120]) as game:
            game.send(' r')
            self.assertEqual(game.board().count('.'), 400)
            game.send(' ')
            self.assertIn(game.board()[0], ' 012345678')
        with TerminalGame([20, 20, 120]) as game:
            os.write(game.master, b' q')
            game.read_until(lambda: game.process.poll() is not None)
            self.assertEqual(game.process.returncode, 0)

    def test_interrupt_and_terminate_restore_terminal(self):
        for end_prompt in (False, True):
            for stop_signal in (signal.SIGINT, signal.SIGTERM):
                with self.subTest(end_prompt=end_prompt, signal=stop_signal):
                    mines = 399 if end_prompt else 120
                    with TerminalGame([20, 20, mines, '--seed', '0']) as game:
                        if end_prompt:
                            game.send(' ')
                            game.read_until(lambda: 'new game [y]  quit [q]' in game.text())
                        os.kill(game.process.pid, stop_signal)
                        game.read_until(lambda: game.process.poll() is not None)
                        self.assertEqual(game.process.returncode, 128 + stop_signal)
                        self.assertIn('\x1b[?1000l', game.raw_output)
                        self.assertIn('\x1b[?1006l', game.raw_output)
                        # TerminalGame.close also checks the original termios state.

    def test_dense_boards_and_first_click_safety(self):
        with TerminalGame([20, 20, 399]) as game:
            game.mouse(0, 33, 12)
            self.assertIn('you win!', game.text())
            game.mouse(0, 33, 12, release=True)
            game.mouse(2, 3, 2)
            self.assertIn('you win!', game.text())
            game.mouse(0, 11, 24)
            self.assertEqual(game.board().count('.'), 400)
        with TerminalGame([20, 20, 120]) as game:
            game.batch([(210, ' ')])
            self.assertNotIn('you lose!', game.text())
            self.assertIn(game.board()[210], ' 012345678')

    def test_win_keyboard_new_game_and_quit(self):
        with TerminalGame([20, 20, 399]) as game:
            game.send(' ')
            self.assertIn('you win!', game.text())
            game.send('y')
            self.assertEqual(game.board().count('.'), 400)
            game.send(' ')
            self.assertIn('you win!', game.text())
            game.send('q')
            game.read_until(lambda: game.process.poll() is not None)
            self.assertEqual(game.process.returncode, 0)

    def test_lose_and_new_game(self):
        # Use a seeded printed layout to deliberately hit a mine in this UI test.
        args = [20, 20, 200, '--seed', '0', '--first', '1,1']
        printed = subprocess.run([BINARY, *map(str, args), '--show'],
            capture_output=True, text=True, check=True)
        cells = ' '.join(printed.stdout.splitlines()[-20:]).split()
        cell = cells.index('*')
        for action in ('mouse-new', 'mouse-quit', 'y', 'q'):
            with self.subTest(action=action), TerminalGame(args) as game:
                game.send(' ')
                game.batch([(cell, 'f')])
                self.assertEqual(game.board()[cell], '@')
                row, col = divmod(cell, 20)
                game.mouse(2, col * 3 + 3, row + 2)
                game.mouse(0, col * 3 + 3, row + 2)
                self.assertIn('you lose!', game.text())
                game.resize(19, 55)
                self.assertIn('new game [y]  quit [q]', game.text())
                # Releases, right clicks and obsolete/outside coordinates do nothing.
                game.mouse(0, 11, 18, release=True)
                game.mouse(2, 11, 18)
                game.mouse(0, 11, 24)
                game.mouse(0, 14, 18)
                self.assertIn('you lose!', game.text())
                if action == 'mouse-new':
                    game.mouse(0, 10, 18)
                elif action == 'mouse-quit':
                    game.mouse(0, 22, 18)
                else:
                    game.send(action)
                if action in ('mouse-new', 'y'):
                    game.resize(32, 100)
                    self.assertEqual(game.board().count('.'), 400)
                else:
                    game.read_until(lambda: game.process.poll() is not None)
                    self.assertEqual(game.process.returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
