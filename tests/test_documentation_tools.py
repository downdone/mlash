"""Broken-navigation and privacy failures must be caught before publishing."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from check_docs import check, targets
from check_document_privacy import inspect


class DocumentationToolsTests(unittest.TestCase):
    def test_moved_document_and_unindexed_contract_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'docs').mkdir()
            (root/'README.md').write_text('[Docs](docs/README.md)',encoding='utf-8')
            (root/'docs/README.md').write_text('[Moved](old-name.md)',encoding='utf-8')
            (root/'docs/contract.md').write_text('# Contract',encoding='utf-8')
            _,errors=check(root,['README.md','docs/README.md','docs/contract.md'])
            self.assertEqual(2,len(errors))
            (root/'docs/README.md').write_text('[Current](contract.md)',encoding='utf-8')
            self.assertEqual([],check(root,['README.md','docs/README.md','docs/contract.md'])[1])

    def test_external_anchor_code_and_reference_links(self):
        text='[Online](https://example.org)\n[Here](#here)\n[ref]: current.md\n```text\n[example](not-a-file.md)\n```\n'
        self.assertEqual(['https://example.org','#here','current.md'],targets(text))

    def test_escape_outside_repository_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'README.md').write_text('[Outside](../private.md)',encoding='utf-8')
            self.assertEqual(1,len(check(root,['README.md'])[1]))

    def test_private_value_is_never_returned_in_report(self):
        value='ExamplePerson'
        findings=inspect('README.md',('C:/Users/'+value+'/workspace').encode())
        self.assertTrue(findings)
        self.assertNotIn(value,repr(findings))
        self.assertEqual([],inspect('README.md',b'Data directory: %LOCALAPPDATA%/Mlash'))


if __name__=='__main__':unittest.main()
