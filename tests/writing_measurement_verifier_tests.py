"""Input-log oracle regressions; expected prefixes are worked by hand."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import json

spec = importlib.util.spec_from_file_location(
    'writing_measurement', Path(__file__).resolve().parents[1] / 'tools/check-writing-responsiveness.py')
measurement = importlib.util.module_from_spec(spec)
spec.loader.exec_module(measurement)


class InputLogTests(unittest.TestCase):
    def setUp(self):
        self.before = {'revision': 1, 'titlePage': [], 'elements': [
            {'id': 'a', 'type': 'action', 'text': 'Rain.'},
            {'id': 'b', 'type': 'action', 'text': 'Bell.'}]}
        self.edits = [{'element_id': 'a', 'offset': 0, 'text': 'soft '},
                      {'element_id': 'b', 'offset': 4, 'text': ' rings'}]

    def test_snapshot_prefixes_and_unchanged_input(self):
        self.assertEqual(measurement.apply_inputs(self.before, self.edits, 0), self.before)
        partial = measurement.apply_inputs(self.before, self.edits, 7)
        self.assertEqual([e['text'] for e in partial['elements']], ['soft Rain.', 'Bell r.'])
        complete = measurement.apply_inputs(self.before, self.edits)
        self.assertEqual([e['text'] for e in complete['elements']], ['soft Rain.', 'Bell rings.'])
        self.assertEqual(self.before['elements'][0]['text'], 'Rain.')
        with self.assertRaises(AssertionError):
            measurement.apply_inputs(self.before, self.edits, 100)

    def test_rejects_wrong_saved_writing_and_stale_snapshot_before_oracles(self):
        result = {'edits': self.edits, 'exports': []}
        with self.assertRaisesRegex(AssertionError, 'Autosave differs'):
            measurement.verify_run(self.before, self.before, result, Path('.'), Path('.'))
        after = measurement.apply_inputs(self.before, self.edits)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # A request after the first phase must include "soft ". A stale
            # first snapshot could otherwise agree with its own PDF/Fountain.
            (root / 'captured.json').write_text(json.dumps(self.before))
            result['exports'] = [{'prefix': 'captured', 'kind': 'pdf', 'keys_before_request': 5}]
            with self.assertRaisesRegex(AssertionError, 'Snapshot differs'):
                measurement.verify_run(self.before, after, result, root, Path('.'))


if __name__ == '__main__':
    unittest.main()
