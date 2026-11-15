"""Verify independently versioned Jazz Notes data without touching frozen firmware."""
import hashlib
import json
from pathlib import Path
import re
import unittest
import zlib

ROOT=Path(__file__).resolve().parents[1]
OVERLAY=ROOT/'editorial/jazz-notes'

def read_data():
 manifest=json.loads((OVERLAY/'manifest.json').read_text('ascii'))
 for relative,record in manifest['files'].items():
  path=OVERLAY/relative
  raw=path.read_bytes()
  if {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}!=record:
   raise ValueError('Frozen editorial SHA mismatch: '+relative)
 return manifest

def check_index(text,index):
 # Exactly the implementation's V2 index contract, not an inferred naming
 # scheme: header CRC for whole TSV and CRC for raw offset body.
 header,sep,body=index.partition(b'\n')
 if not sep:raise ValueError('Missing binary/ASCII jazz index header')
 fields=header.decode('ascii').split('\t')
 if len(fields)!=6 or fields[:2]!=['#PJ-JAZZ-IDX','2']:
  raise ValueError('Invalid V2 jazz index header')
 count=int(fields[3]);length=int(fields[2]);rows=text.splitlines(keepends=True);offsets=body.splitlines()
 if (length!=len(text) or count!=1498 or len(rows)!=count or len(offsets)!=count or
     f'{zlib.crc32(text)&0xffffffff:08x}'!=fields[4] or
     f'{zlib.crc32(body)&0xffffffff:08x}'!=fields[5]):
  raise ValueError('TSV/index mismatch or damaged checksum')
 pos=0
 for data,offset in zip(rows,offsets):
  parts=data.rstrip(b'\r\n').split(b'\t')
  if len(parts)!=5 or b'\r' in data or not data.endswith(b'\n'):
   raise ValueError('Offline row changed field count or CRLF')
  actual=offset.decode('ascii').split('\t')
  if len(actual)!=2 or actual!=[parts[0].decode('ascii'),str(pos)]:
   raise ValueError('Offset/artist ID mismatch in generated V2 index')
  pos+=len(data)
 if pos!=len(text):raise ValueError('End offset mismatch')
 return count

class IndependentJazzEditorialOverlay(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.manifest=read_data()

 def test_manifests_cover_six_independent_source_files(self):
  m=self.manifest
  self.assertEqual(m['catalog_entries'],1498)
  self.assertEqual(m['individually_source_reviewed'],983)
  self.assertEqual(m['researched_extra'],920)
  self.assertEqual(m['source_dossiers'],833)
  self.assertEqual(m['claims_individually_attributed'],1820)
  self.assertEqual(m['pending_not_source_reviewed'],515)
  self.assertEqual(len(m['files']),6)
  self.assertEqual(sum(r['bytes'] for r in m['files'].values()),
    sum((OVERLAY/k).stat().st_size for k in m['files']))
  self.assertNotIn('firmware/', '\n'.join(m['files']))

 def test_index_matches_every_1498_original_artist_and_byte_position(self):
  t=(OVERLAY/'sd/jazz-encyclopedia.tsv').read_bytes()
  i=(OVERLAY/'sd/jazz-encyclopedia.idx').read_bytes()
  self.assertEqual(check_index(t,i),1498)
  invalid=t.replace(b'A. R. Penck',b'A. R. Penco',1)
  with self.assertRaisesRegex(ValueError,'checksum'):
   check_index(invalid,i)

 def test_source_biographies_are_exact_on_1498_row_offline_data(self):
  lines=(OVERLAY/'sd/jazz-encyclopedia.tsv').read_text('ascii').splitlines()
  catalog={p[0]:p for line in lines if len(p:=line.split('\t'))==5}
  self.assertEqual(len(catalog),1498)
  raw=(OVERLAY/'source/jazz-researched-context.tsv').read_text('ascii').splitlines()
  self.assertEqual(len(raw),920)
  for row in raw:
   key,short,source=row.split('\t')
   self.assertIn(key,catalog)
   self.assertEqual(catalog[key][2],short)
   self.assertTrue(80<=len(short)<=180 and short.isascii())
   self.assertTrue(source.startswith('https://'))

 def test_all_833_individual_dossiers_link_to_specific_claims(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  self.assertEqual(len(docs),833)
  claim_sum=0
  for key,doc in docs.items():
   with self.subTest(key=key):
    proof=doc['evidence']
    self.assertGreaterEqual(len(proof),2)
    self.assertEqual([row['id'] for row in proof],
                     [f'ev{i:02d}' for i in range(1,len(proof)+1)])
    self.assertIn(doc['primary_source_url'],{r['source_url'] for r in proof})
    self.assertTrue(all(isinstance(r['claim'],str) and
                      len(r['claim'])>=20 and r['source_url'].startswith('https://') for r in proof))
    claim_sum+=len(proof)
  self.assertEqual(claim_sum,1820)

 def test_generated_future_source_has_983_exact_hints(self):
  header=(OVERLAY/'source/JazzNowPlaying.h').read_text('ascii')
  self.assertIn('static constexpr size_t kCuratedCount = 983;',header)
  self.assertIn('Exact ICY matches only.',header)
  self.assertIn('static bool sameName',header)
  self.assertIn('return !*a && !*b',header)
  self.assertIn('Cassino Simpson',header)
  self.assertIn('Hagood Hardy',header)

 def test_frozen_public_firmware_source_was_not_replaced(self):
  self.assertTrue((ROOT/'firmware/SHA256-SOURCES.txt').is_file())
  version=(ROOT/'firmware/PocketJazzRadio/BuildVersion.h').read_text('ascii')
  self.assertIn('#define POCKETJAZZ_FIRMWARE_VERSION "v0.8.0"',version)
  self.assertNotEqual((ROOT/'firmware/data/jazz-encyclopedia.tsv').read_bytes(),
                      (OVERLAY/'sd/jazz-encyclopedia.tsv').read_bytes())

if __name__=='__main__':unittest.main()
