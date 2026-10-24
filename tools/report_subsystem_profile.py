#!/usr/bin/env python3
"""Summarize isolated inclusive SGDK timer probes, with explicit scope limits."""
import argparse
import hashlib
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
directory = args.directory
scope = json.loads((directory/'profile-scope.json').read_text())
manifest = json.loads((directory/'out/build-manifest.json').read_text())
runtime = json.loads((directory/'out/genplus-results.json').read_text())
rom_hash = hashlib.sha256((directory/'out/rom.bin').read_bytes()).hexdigest()
assert manifest['rom_sha256'] == rom_hash, 'Build/ROM mismatch'
reports = []
for run in runtime:
    assert run['rom_sha256'] == rom_hash, 'Runtime/ROM mismatch'
    players = run['players']
    log = (directory/f'out/genplus-{players}p.txt').read_text()
    rows = [tuple(map(int, row)) for row in
            re.findall(r'^PROFILE (\d+) (\d+) (\d+) (\d+)$', log, re.M)]
    assert [row[0] for row in rows] == list(range(len(scope['slots']))), 'Missing/duplicate probe'
    probes = []
    for slot, total, count, peak in rows:
        # Totals may include an interrupted outer call's completed child. Do
        # not require equal parent/child counts or subtract them as exact costs.
        assert total >= peak and (count > 0 or total == peak == 0), 'Invalid completed-call data'
        assert peak < 76800, 'At least one second in a probe: inspect stall/timer discontinuity'
        probes.append(dict(name=scope['slots'][slot], total_subticks=total,
                           calls=count, mean_subticks=total/count if count else None, max_subticks=peak))
    reports.append(dict(players=players, rom_sha256=rom_hash,
                        simulation_elapsed=run['simulation_elapsed'], probes=probes))
report = dict(scope=scope['scope'], counters=scope['counters'],
              interpretation='Inclusive totals overlap. Compare totals and call counts; per-call averages alone hide alternating-player cadence. Timer probes perturb execution and are not release FPS or exact CPU cycles.',
              runs=reports)
(directory/'profile-results.json').write_text(json.dumps(report, indent=2)+'\n')
for run in reports:
    print(f"{run['players']}P diagnostic {run['rom_sha256']}")
    for probe in run['probes']:
        mean = f"{probe['mean_subticks']:.1f}" if probe['calls'] else 'not sampled'
        print(f"  {probe['name']}: {probe['calls']} calls, "
              f"mean {mean}, max {probe['max_subticks']} subticks")
