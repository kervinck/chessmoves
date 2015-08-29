#!/usr/bin/env python

import chessmoves as cm

print cm.moves('2r3r1/3b1p1k/1p2pPp1/2npP1Pp/p2N3P/P4B2/KPPR4/3R4 w - -').keys()
print cm.moves('2r3r1/3b1p1k/1p2pPp1/2npP1Pp/p2N3P/P4B2/KPPR4/3R4 w - -')['b4']
print cm.moves('2r3r1/3b1p1k/1p2pPp1/2npP1Pp/pP1N3P/P4B2/K1PR4/3R4 b - b3').keys()
print cm.moves('2r3r1/3b1p1k/1p2pPp1/2npP1Pp/pP1N3P/P4B2/K1PR4/3R4 b - b3')['Rb8']
print cm.moves('2r3r1/3b1p1k/1p2pPp1/2npP1Pp/pP1N3P/P4B2/K1PR4/3R4 b -').keys()

for pos, ref in [
        ('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -', 0x463b96181691fc9c),
        ('rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3', 0x823c9b50fd114196),
        ('rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6', 0x0756b94461c50fb0),
        ('rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR b KQkq -', 0x662fafb965db29d4),
        ('rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6', 0x22a48b5a8e47ff78),
        ('rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR b kq -', 0x652a607ca3f242c1),
        ('rnbq1bnr/ppp1pkpp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR w - -', 0x00fdd303c946bdd9),
        ('rnbqkbnr/p1pppppp/8/8/PpP4P/8/1P1PPPP1/RNBQKBNR b KQkq c3', 0x3c8123ea7b067637),
        ('rnbqkbnr/p1pppppp/8/8/P6P/R1p5/1P1PPPP1/1NBQKBNR b Kkq -', 0x5c3f9b829b279560)
        ]:
        hash = cm.hash(pos)
        result = 'OK' if hash == ref else 'NOK'
        print '0x%016x [ref: 0x%016x] %s %s' % (hash, ref, result, pos)

# Test move parsing

parsePos = '6k1/1P6/8/b1PpP3/4PN2/2N5/8/R3K2R w KQ d6'

for move in [
        'Ke2', 'Ke', 'cd', 'exd5', 'exd6', 'RxB', 'xB', 'NxP',
        'foo', 'NP', '123', 'abc',
        'Ae2', 'b', '78', '8', '7b',
        'exd',
        'b8=Q', 'b8', 'b7b8', 'b8=N', 'b8=R', 'b8=B',
        'b7b8q', 'b7b8r', 'b7b8b', 'b7b8n',
        'b8=A', 'bKe', 'cxd', 'exd',
        'O-O', 'O-O-O', 'OO', 'OOO',
        'o-o', 'o-o-o', 'oo', 'ooo',
        '0-0', '0-0-0', '00', '000',
        'O-O-0', 'o-o-o-o', 'o-oo', 'oo-o', 'O-O-', 'o', '0', 'O', 'O--O']:
        try:
                print 'parse:', pos, move, '->', cm.move(parsePos, move)
        except ValueError as err:
                print err
