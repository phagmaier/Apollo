# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Disposable current-writer timings and independent export verification.

Called by check-writing-responsiveness.sh. All artifacts stay in its new output
folder. Synthetic Qt keys are not device/compositor input timing. First-process
observations do not flush OS caches and are not a true cold-machine benchmark.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from editor_pdf_tests import editor_pages
from pdf_tests import check_pdf
from fountain_export_process_tests import aw_reading, model, presented, presented_spacing


def command(*args, env=None, timeout=300):
    result = subprocess.run([str(a) for a in args], env=env, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f'{args[0]} failed ({result.returncode}): {result.stdout}\n{result.stderr}')
    return result.stdout


def apply_inputs(before, edits, keys=None):
    expected = copy.deepcopy(before)
    remaining = sum(len(edit['text']) for edit in edits) if keys is None else keys
    assert remaining >= 0
    for edit in edits:
        target, = [e for e in expected['elements'] if e['id'] == edit['element_id']]
        offset = edit['offset']
        typed = edit['text'][:remaining]
        assert 0 <= offset <= len(target['text'])
        target['text'] = target['text'][:offset] + typed + target['text'][offset:]
        remaining -= len(typed)
        if remaining == 0:
            break
    assert remaining == 0, 'Snapshot capture exceeds input log'
    return expected


def verify_run(before, after, result, artifacts, parser):
    expected = apply_inputs(before, result['edits'])
    # Compare all authored data and IDs; only revision advances.
    expected['revision'] = after['revision']
    assert after == expected, 'Autosave differs from independently applied input log'
    verified = {'pdf': 0, 'fountain': 0}
    for event in result['exports']:
        prefix = artifacts / event['prefix']
        captured = json.loads(prefix.with_suffix('.json').read_text())
        snapshot = apply_inputs(before, result['edits'], event['keys_before_request'])
        snapshot['revision'] = captured['revision']
        assert captured == snapshot, 'Snapshot differs from input prefix at request time'
        if event['kind'] == 'pdf':
            pages, _ = editor_pages(prefix.with_suffix('.tsv'))
            check_pdf(prefix.with_suffix('.pdf'), pages)
        else:
            actual = aw_reading('node', parser, prefix.with_suffix('.fountain'))
            assert actual == model(presented_spacing(presented(captured))), f'Fountain oracle mismatch: {prefix}'
        verified[event['kind']] += 1
    assert all(verified.values()), 'Both export formats must be exercised'
    return verified


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--keys', type=int, default=600, help='keys per phase, three phases per workload')
    p.add_argument('--smoke', action='store_true', help='primary workload, short harness verification; no budget acceptance')
    args = p.parse_args()
    assert args.output.is_dir() and not (args.output / 'summary.json').exists()
    assert 4 <= args.keys <= 2000
    editor = ROOT / 'build/release/apollo_editor'
    harness = ROOT / 'build/release/measure_fountain_workflow'
    cache = Path(os.environ.get('XDG_CACHE_HOME', Path.home() / '.cache'))
    parser = cache / 'apollo/afterwriting/node_modules/.pnpm/aw-parser@0.1.1/node_modules/aw-parser'
    assert (parser / 'parser.js').is_file(), 'Install the pinned oracle with tools/afterwriting-oracle.sh first'
    results = []
    for scenes in ((175,) if args.smoke else (175, 444)):
        output = args.output / str(scenes)
        output.mkdir(mode=0o700)
        source = output / 'generated.fountain'
        generated = command('uv', 'run', ROOT / 'tools/generate_script.py', '--scenes', scenes, '--seed', 1)
        # The unchanged bounded exporter requires an uppercase title. Normalize
        # only this generated workload's metadata, never an imported document.
        generated = generated.replace(f'Title: Synthetic Workload {scenes}\n',
                                      f'Title: SYNTHETIC WORKLOAD {scenes}\n', 1)
        source.write_text(generated)
        project = output / 'project'
        headless = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', QSG_RENDER_LOOP='basic')
        command(editor, project, '--create', source, env=headless)
        writing = project / 'screenplay.json'
        prepared = json.loads(writing.read_text())
        lyric_count = 0
        for element in prepared['elements']:
            if element['type'] == 'lyric':
                # --create's spike parser adds implicit italics as authored
                # spans. Ordinary type authoring does not. Prepare that model.
                assert element.get('styles') == [{'start': 0, 'end': len(element['text']), 'style': 'italic'}]
                del element['styles']
                lyric_count += 1
        writing.write_text(json.dumps(prepared) + '\n')
        command(editor, project, '--export-fountain', output / 'baseline.fountain', env=headless)
        before = json.loads(writing.read_text())
        launch = time.monotonic_ns()
        process = subprocess.run([str(harness), '--project', str(project), '--output', str(output),
                                  '--keys', str(args.keys), '--launch-ns', str(launch)],
                                 capture_output=True, text=True, timeout=240)
        (output / 'stdout.json').write_text(process.stdout)
        (output / 'stderr.log').write_text(process.stderr)
        assert process.returncode == 0, f'Measurement failed: {process.stdout}\n{process.stderr}'
        result = json.loads(process.stdout)
        assert result['success'], result
        after = json.loads((project / 'screenplay.json').read_text())
        result['independent_outputs_verified'] = verify_run(before, after, result, output / 'run', parser)
        if not args.smoke:
            assert all(phase['actual_worker_overlap_key_to_frame']['samples'] > 0
                       for phase in result['phases'] if phase['mode'] != 'typing'), 'No actual export-worker overlap'
        result['scenes'] = scenes
        result['prepared_implicit_lyric_spans_removed'] = lyric_count
        result['prepared_generated_title_uppercase'] = True
        result['primary_budget_met'] = (
            result['platform'] == 'wayland' and result['renderer'] == 'OpenGL'
            and result['warm_reopen_to_writer_frame_ms'] <= 1000
            and result['memory']['sampled_editor_plus_helper_max_pss_mib'] <= 250
            and all(phase['key_to_frame_including_export_request']['p95_ms'] <= 33
                    and phase['stalls_over_100_ms'] == 0
                    and (phase['mode'] == 'typing' or phase['actual_worker_overlap_key_to_frame']['samples'] > 0) for phase in result['phases'])
            and all(e['total_ms'] <= 5000 for e in result['exports'] if e['kind'] == 'pdf'))
        results.append(result)
        (output / 'verified.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps({k: result[k] for k in ('scenes', 'pages', 'primary_budget_met', 'independent_outputs_verified')}), flush=True)
    summary = {'runs': results, 'smoke_only': args.smoke,
               'limits': 'Qt synthetic dispatch through synchronized frameSwapped; no physical/compositor-input or owner-comfort claim. '
                         'Process-first observation with warm OS caches, not a true cold-machine start. '
                         'GUI-timer sampled PSS includes helpers when observable, not a peak guarantee. '
                         'Expected-snapshot copying, line dumps and artifact writes are outside individual key timings; included in session duration. '
                         'Full hand-derived layout fixtures are checked separately by mise run check.'}
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Results: {args.output / "summary.json"}', flush=True)
    if not args.smoke and not results[0]['primary_budget_met']:
        raise SystemExit('Primary performance budget missed; retain measurements and investigate before acceptance')


if __name__ == '__main__':
    main()
