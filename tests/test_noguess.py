#!/usr/bin/env python3
"""No-guess CLI contracts and terminal integration, using the real executable."""
import os
import signal
import subprocess
import unittest

from test_cli import BINARY, TerminalGame
from test_show import audit, parse_board


def invoke(*args):
    return subprocess.run([BINARY, *map(str, args)], stdin=subprocess.DEVNULL,
        capture_output=True, text=True, timeout=15,
        env={**os.environ, 'TERM': 'dumb', 'LC_ALL': 'C', 'NO_COLOR': '1'})


def show(rows, columns, total, first, seed=0, extra=()):
    return invoke(rows, columns, total, '--mode=no-guess', '--show', '--seed', seed,
        '--first', f'{first[0]},{first[1]}', *extra)


class NoGuessShowTests(unittest.TestCase):
    def test_fixed_count_first_zero_and_reproducible_layouts(self):
        for rows, columns, total, first, seed in [
                (9, 9, 10, (1, 1), 0), (9, 12, 20, (5, 6), 37),
                (16, 16, 40, (16, 16), 4294967295)]:
            with self.subTest(size=(rows, columns), first=first, seed=seed):
                a = show(rows, columns, total, first, seed)
                b = show(rows, columns, total, first, seed)
                self.assertEqual(a.returncode, 0, a.stderr)
                self.assertEqual(b.returncode, 0, b.stderr)
                self.assertNotIn('\x1b', a.stdout + a.stderr + b.stdout + b.stderr)
                board = parse_board(a.stdout, rows, columns)
                self.assertEqual(board, parse_board(b.stdout, rows, columns))
                index = (first[0] - 1) * columns + first[1] - 1
                audit(board, rows, columns, total, index)
                self.assertEqual(board[index], '.')
                self.assertIn('mode=no-guess', a.stdout)

    def test_default_mode_matches_explicit_no_guess_and_accepts_budgets(self):
        args = [9, 12, 20, '--show', '--seed', '37', '--first', '5,6',
            '--max-attempts', '512', '--generation-timeout', '5000']
        implicit = invoke(*args)
        explicit = invoke(*args, '--mode=no-guess')
        self.assertEqual(implicit.returncode, 0, implicit.stderr)
        self.assertEqual(explicit.returncode, 0, explicit.stderr)
        self.assertIn('mode=no-guess', implicit.stdout)
        self.assertIn('mode=no-guess', explicit.stdout)
        self.assertEqual(parse_board(implicit.stdout, 9, 12),
            parse_board(explicit.stdout, 9, 12))

    def test_both_modes_share_the_initial_candidate(self):
        # A single-mine board needs no repair, so accepting the first candidate
        # lets the executable expose whether both modes use the same shuffle.
        random = invoke(9, 9, 1, '--mode=random', '--show', '--seed', '37', '--first', '1,1')
        no_guess = show(9, 9, 1, (1, 1), seed=37)
        self.assertEqual(random.returncode, 0, random.stderr)
        self.assertEqual(no_guess.returncode, 0, no_guess.stderr)
        self.assertIn('attempts=1', no_guess.stdout)
        self.assertEqual(parse_board(random.stdout, 9, 9), parse_board(no_guess.stdout, 9, 9))

    def test_invalid_arguments_fail_before_entering_terminal_mode(self):
        invalid = [('--mode=unknown',), ('--mode=',),
            ('--mode=random', '--max-attempts', '1'),
            ('--mode=random', '--generation-timeout', '1')]
        for flag in ('--max-attempts', '--generation-timeout'):
            for value in ('0', '-1', '1x', '2147483648'):
                invalid.append(('--mode=no-guess', flag, value))
        for args in invalid:
            with self.subTest(args=args):
                result = invoke(9, 9, 10, '--show', *args)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('\x1b', result.stdout + result.stderr)
        for total in (0, 78, 80):
            result = show(9, 9, total, (1, 1))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('1..77', result.stderr)
            self.assertEqual(result.stdout, '')

    def test_infeasible_first_zero_and_exhaustion_never_print_a_board(self):
        infeasible = show(9, 9, 73, (5, 5))
        self.assertNotEqual(infeasible.returncode, 0)
        self.assertIn('first zero supports 1..72', infeasible.stderr)
        self.assertEqual(infeasible.stdout, '')
        # A complete-board attempt budget gives a deterministic failure, without
        # relying on a narrow timing threshold or assuming all densities work.
        exhausted = show(20, 20, 200, (11, 11), extra=(
            '--max-attempts', '1', '--generation-timeout', '5000'))
        self.assertNotEqual(exhausted.returncode, 0)
        self.assertIn('budget exhausted after 1 attempts', exhausted.stderr)
        self.assertEqual(exhausted.stdout, '')
        self.assertNotIn('\x1b', infeasible.stderr + exhausted.stderr)


class NoGuessTerminalTests(unittest.TestCase):
    def test_short_board_preserves_help_and_clears_generation_message(self):
        with TerminalGame([9, 9, 10, '--seed', '0',
                '--first', '1,1'], 9, 9) as game:
            self.assertIn('mode       :no-guess', game.text())
            game.send(' ')
            game.read_until(lambda: game.board()[0] != '.')
            self.assertEqual(game.board()[0], ' ')
            for help_line in ('pan        :LMB edge', 'flag       :j/f,RMB',
                    'sweep      :space,LMB', 'restart    :r', 'quit       :q'):
                self.assertIn(help_line, game.text())
            self.assertIn('Generating no-guess', game.raw_output)
            for leftover in ('Generating', 'q:quit', 'r:cancel'):
                self.assertNotIn(leftover, game.text())

    def test_seeded_game_matches_show_and_restart_can_generate_again(self):
        rows, columns, total, first = 9, 12, 20, (5, 6)
        printed = show(rows, columns, total, first, seed=37)
        self.assertEqual(printed.returncode, 0, printed.stderr)
        expected = parse_board(printed.stdout, rows, columns)
        first_index = (first[0] - 1) * columns + first[1] - 1
        args = [rows, columns, total, '--seed', '37',
            '--first', f'{first[0]},{first[1]}']
        with TerminalGame(args, rows, columns) as game:
            game.send('f ')
            self.assertEqual(game.board()[first_index], '@')
            self.assertNotIn('Generating no-guess', game.raw_output)
            game.send('f ')
            game.read_until(lambda: game.board()[first_index] != '.')
            self.assertEqual(game.board()[first_index], ' ')
            before_resize = game.board()
            game.resize(19, 55)
            game.resize(32, 100)
            self.assertEqual(game.board(), before_resize)
            for cell, value in enumerate(expected):
                if value != '*' and game.board()[cell] == '.':
                    game.batch([(cell, ' ')])
            game.read_until(lambda: 'you win!' in game.text())
            self.assertNotIn('you lose!', game.text())
            for cell, value in enumerate(expected):
                if value != '*':
                    self.assertEqual(int(game.board()[cell].strip() or 0),
                        int(value.replace('.', '0')))
            game.send('y')
            self.assertEqual(game.board().count('.'), rows * columns)
            game.batch([(first_index, ' ')])
            game.read_until(lambda: game.board()[first_index] != '.')
            self.assertEqual(game.board()[first_index], ' ')
            game.send('r')
            self.assertEqual(game.board().count('.'), rows * columns)

    def test_cancel_and_quit_while_generating_restore_terminal(self):
        args = [20, 20, 200, '--mode=no-guess', '--seed', '0', '--first', '11,11']
        with TerminalGame(args) as game:
            # Queue cancellation with the opening key, so this exercises the
            # generation callback even when the first attempt finishes quickly.
            os.write(game.master, b' r')
            game.read_until(lambda: 'Generation cancelled' in game.text())
            self.assertEqual(game.board().count('.'), 400)
            self.assertIsNone(game.process.poll())
        with TerminalGame(args) as game:
            os.write(game.master, b' q')
            game.read_until(lambda: game.process.poll() is not None)
            self.assertEqual(game.process.returncode, 0)
            self.assertIn('Generating no-guess', game.raw_output)
        # TerminalGame.close validates both termios and mouse-mode restoration.

    def test_exhaustion_keeps_board_hidden_and_allows_retry_and_restart(self):
        args = [20, 20, 200, '--seed', '0', '--first', '11,11',
            '--max-attempts', '1', '--generation-timeout', '5000']
        with TerminalGame(args) as game:
            game.send(' ')
            game.read_until(lambda: 'No-guess budget exhausted' in game.text())
            self.assertEqual(game.board().count('.'), 400)
            generated_messages = game.raw_output.count('Generating no-guess')
            game.send(' ')
            game.read_until(lambda: game.raw_output.count('Generating no-guess') > generated_messages)
            game.read_until(lambda: 'No-guess budget exhausted' in game.text())
            self.assertEqual(game.board().count('.'), 400)
            game.send('r')
            self.assertEqual(game.board().count('.'), 400)
            self.assertNotIn('No-guess budget exhausted', game.text())

    def test_infeasible_first_cell_leaves_game_available(self):
        for mode in ('random', 'no-guess'):
            with self.subTest(mode=mode), TerminalGame([9, 9, 73, f'--mode={mode}',
                    '--first', '5,5'], 9, 9) as game:
                game.send(' ')
                self.assertIn('First zero supports 1..72', game.text())
                self.assertEqual(game.board().count('.'), 81)
                self.assertIsNone(game.process.poll())
                game.batch([(0, ' ')])
                game.read_until(lambda: game.board()[0] != '.' or 'budget exhausted' in game.text())
                self.assertNotIn('choose another cell', game.text())
                self.assertNotIn('you lose!', game.text())

    def test_resize_and_termination_during_generation(self):
        args = [20, 20, 200, '--mode=no-guess', '--seed', '0', '--first', '11,11']
        with TerminalGame(args) as game:
            os.write(game.master, b' ')
            game.resize(19, 55)
            game.read_until(lambda: 'warning:clipped' in game.text())
            game.resize(32, 100)
            game.read_until(lambda: 'Generating no-guess' in game.raw_output)
            os.kill(game.process.pid, signal.SIGTERM)
            game.read_until(lambda: game.process.poll() is not None)
            self.assertEqual(game.process.returncode, 128 + signal.SIGTERM)


if __name__ == '__main__':
    unittest.main(verbosity=2)
