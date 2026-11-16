"""Verify independently versioned Jazz Notes data without touching frozen firmware."""
import hashlib
import json
from pathlib import Path
import re
import unittest
import zlib
from urllib.parse import urlsplit

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
  self.assertEqual(m['individually_source_reviewed'],1190)
  self.assertEqual(m['researched_extra'],1127)
  self.assertEqual(m['source_dossiers'],1040)
  self.assertEqual(m['claims_individually_attributed'],2437)
  self.assertEqual(m['pending_not_source_reviewed'],308)
  self.assertEqual(m['wave30_new_individual'],16)
  self.assertEqual(m['wave30_new_claims'],48)
  self.assertEqual(m['remaining_by_category'],
                   {'evidence_candidate':183,'insufficient_evidence':79,'ambiguous':46})
  self.assertEqual(m['wave30_promotions_by_previous_category'],
                   {'evidence_candidate':16,'insufficient_evidence':0,'ambiguous':0})
  self.assertEqual(m['research_stage_sha256'],'562d68d72c770450de2d2be5827cadddaa6f65ac86df322d9258514ddf4789bf')
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
  self.assertEqual(len(raw),1127)
  for row in raw:
   key,short,source=row.split('\t')
   self.assertIn(key,catalog)
   self.assertEqual(catalog[key][2],short)
   self.assertTrue(80<=len(short)<=180 and short.isascii())
   self.assertTrue(source.startswith('https://'))

 def test_all_1040_individual_dossiers_link_to_specific_claims(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  self.assertEqual(len(docs),1040)
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
  self.assertEqual(claim_sum,2437)

 def test_wave30_16_personal_source_histories_not_broadened_to_unverified_credits(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  keys=('al-killian','buddy-banks','christian-garros','aage-tanggaard','george-e-lee',
        'johnny-scott','marc-edwards','marcel-zanini','matty-malneck','pete-briggs',
        'tom-talbert','virgil-gonsalves','simon-brehm','uli-trepte',
        'steve-harris','shinji-takeda')
  self.assertEqual(len(keys),16)
  self.assertEqual(len(set(keys)),16)
  self.assertTrue(set(keys)<=set(docs))
  self.assertEqual(sum(len(docs[key]['evidence']) for key in keys),48)
  self.assertNotIn('bud-scott',docs)
  self.assertEqual(len(docs['matty-malneck']['evidence']),2)
  self.assertEqual(len(docs['marc-edwards']['evidence']),4)
  self.assertEqual(len(docs['aage-tanggaard']['evidence']),3)
  self.assertIn('2022',docs['aage-tanggaard']['evidence'][2]['claim'])
  self.assertIn('2022',docs['shinji-takeda']['summary'])

 def test_wave29_20_source_reviews_with_independent_disambiguations(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  keys=('buddy-greco','cal-collins','clarence-williams','frank-froeba',
        'fred-elizalde','freddie-jenkins','george-duke','gil-fuller',
        'henry-kaiser','irene-reid','mark-isham','musa-kaleem',
        'nick-evans','orrin-evans','peter-appleyard','randy-jackson',
        'sammy-davis-jr','sol-yaged','sterling-bose','wyatt-ruther')
  self.assertEqual(len(keys),20)
  self.assertEqual(len(set(keys)),20)
  self.assertTrue(set(keys)<=set(docs))
  self.assertEqual(sum(len(docs[k]['evidence']) for k in keys),59)
  for key in ('buddy-greco','george-duke','orrin-evans'):
   identity=docs[key]['identity_resolution']
   self.assertIn('MBID',identity['note'])
   domains={urlsplit(u).netloc for u in identity['authorities']}
   self.assertGreaterEqual(len(domains),3)
  self.assertNotIn('Metronome-1944-11.pdf',
     ' '.join(e['source_url'] for e in docs['frank-froeba']['evidence']))
  self.assertNotIn('findingaids.loc.gov',
     ' '.join(e['source_url'] for e in docs['sterling-bose']['evidence']))
  self.assertNotIn('worldradiohistory',
     ' '.join(e['source_url'] for e in docs['fred-elizalde']['evidence']))

 def test_wave28_23_new_source_backed_histories_and_no_unverified_rights(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  keys=('chino-pozo','adele-girard','bill-doggett','anthony-ortega',
        'george-williams','gloria-lynne',
        'kenneth-kapstad','merrill-moore','minor-hall','paul-gayten',
        'lonnie-donegan','lucky-millinder','june-christy','marty-marsala',
        'joe-darensbourg','teddy-stauffer','sam-ulano','scheila-gonzalez',
        'ray-anthony','virgilio-savona','thomas-a-dorsey','vinnie-burke',
        'roy-montrell')
  self.assertEqual(len(keys),23)
  self.assertEqual(len(set(keys)),23)
  self.assertTrue(set(keys)<=set(docs))
  self.assertEqual(sum(len(docs[k]['evidence']) for k in keys),68)
  self.assertEqual(len(docs['ray-anthony']['evidence']),2)
  self.assertNotIn('findingaids.loc.gov',
                   ' '.join(e['source_url'] for e in docs['ray-anthony']['evidence']))
  self.assertIn('MBID',docs['kenneth-kapstad']['identity_resolution']['note'])
  self.assertEqual(len({urlsplit(u).netloc for u in
                        docs['kenneth-kapstad']['identity_resolution']['authorities']}),3)
  self.assertNotIn('1960',docs['chino-pozo']['summary'])

 def test_wave27_24_source_claim_chains_and_distinct_ambiguous_identity(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  keys=('george-matthews','herman-autrey','george-kelly','gloria-coleman',
        'page-cavanaugh','johnny-bothwell','min-leibrook','matt-garrison',
        'mary-oliver','nat-peck','nikki-iles','luther-henderson',
        'michael-marcus','merritt-brunies','steffen-weber',
        'victor-noriega','stjepko-gut','roger-dean','sandy-mosse',
        'rob-van-kreeveld','totti-bergh','victor-sproles','skeeter-best',
        'teddy-smith')
  self.assertEqual(len(keys),24)
  self.assertEqual(len(set(keys)),24)
  self.assertTrue(set(keys)<=set(docs))
  self.assertEqual(sum(len(docs[k]['evidence']) for k in keys),74)
  self.assertIn('MBID',docs['nikki-iles']['identity_resolution']['note'])
  domains={urlsplit(url).netloc for url in
           docs['nikki-iles']['identity_resolution']['authorities']}
  self.assertEqual(len(domains),3)
  self.assertNotIn('worldradiohistory',
                   ' '.join(e['source_url'] for e in docs['page-cavanaugh']['evidence']))
  self.assertNotIn('bluesandjazzmart',
                   ' '.join(e['source_url'] for e in docs['sandy-mosse']['evidence']))

 def test_wave26_28_exact_source_histories_and_disambiguated_trumpeter(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  new=('buddy-catlett','christian-chevallier','frank-gant','gene-taylor',
       'georg-riedel','harry-arnold','harry-james','hod-obrien',
       'johnny-dunn','preston-jackson','mark-soskin','jim-robinson',
       'papa-celestin','kippie-moeketsi','john-young','larry-kohut',
       'jimmy-dorsey','kenny-rankin','si-zentner','tarus-mateen',
       'roger-cicero','randy-crawford','zachary-breaux','skitch-henderson',
       'shunzo-ohno','thomas-lang','renee-olstead','stoyan-yankulov')
  self.assertEqual(len(set(new)),28)
  self.assertTrue(set(new)<=set(docs))
  self.assertEqual(sum(len(docs[k]['evidence']) for k in new),83)
  self.assertIn('MBID',docs['shunzo-ohno']['identity_resolution']['note'])
  self.assertNotIn('1973',docs['shunzo-ohno']['summary'])
  self.assertNotIn('1974',docs['shunzo-ohno']['summary'])
  self.assertEqual(docs['georg-riedel']['primary_source_url'],
                   'https://www.digmusic.se/SvenskJazzhistoria.JanAllan.GeorgRiedel.html')

 def test_wave25_all_51_specific_dossiers_and_nonreciprocal_identity_authorities(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  new=set(('anthony-jackson bill-tapia charles-flores dominique-eade don-abney '
   'francesco-buzzurro frank-emilio-flynn gideon-nxumalo isabelle-antena '
   'jackie-paris jeri-southern jimmy-jones joe-benjamin john-malachi julie-london '
   'leo-wright leonid-utyosov lowell-davidson maki-asakawa marlon-jordan '
   'martin-denny masha-bijlsma melvin-rhyne michiel-braam mickey-bass mike-carr '
   'mildred-anderson mocean-worker moses-taiwa-molelekwa nadi-qamar '
   'neal-creque nick-travis paolo-conte paul-smith peggy-gilbert putte-wickman '
   'red-mitchell reginald-robinson roy-ayers roy-milton sadao-watanabe '
   'sammy-nestico susanne-abbuehl terry-callier tom-malone tony-sbarbaro '
   'tyrone-brown walter-dyett wayne-henderson wouter-hamel ziggy-elman').split())
  self.assertEqual(len(new),51)
  self.assertTrue(new<=set(docs))
  self.assertEqual(sum(len(docs[k]['evidence']) for k in new),152)
  for key in ('sadao-watanabe','walter-dyett','masha-bijlsma',
              'putte-wickman','frank-emilio-flynn','terry-callier'):
   self.assertGreaterEqual(len(docs[key]['identity_resolution']['authorities']),2)
  self.assertIn('MBID',docs['sadao-watanabe']['identity_resolution']['note'])
  self.assertEqual(len(docs['terry-callier']['evidence']),2)
  self.assertNotIn('worldradiohistory',str(docs['terry-callier']['evidence']))

 def test_wave24_specific_source_claims_without_disputed_dates(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  wave24=('teo-macero','nick-brignola','morten-qvenild','hilton-ruiz',
          'ernie-wilkins','hermeto-pascoal','mike-melvoin','lennie-tristano',
          'george-bohanon','buddy-deppenschmidt')
  self.assertEqual(len(wave24),10)
  self.assertTrue(set(wave24)<=set(docs))
  self.assertEqual(sum(len(docs[k]['evidence']) for k in wave24),30)
  self.assertIn('1967',docs['nick-brignola']['limitations'])
  self.assertIn('1968',docs['nick-brignola']['limitations'])
  self.assertNotIn('1967',docs['nick-brignola']['summary'])
  self.assertNotIn('1968',docs['nick-brignola']['summary'])
  self.assertEqual(docs['george-bohanon']['primary_source_url'],
                   'https://www.freshsoundrecords.com/george-bohanon-albums/46956-boss-bossa-nova.html')

 def test_wave23_names_have_exact_music_evidence(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  wave23=('anselmo-sacasas','frank-rehak','frank-tusa','gary-boyle',
          'ian-underwood','jeff-clyne','john-eaton','john-guerin','juju',
          'major-holley','marshal-royal','mickey-tucker','yolande-bavan')
  self.assertEqual(len(wave23),13)
  self.assertTrue(set(wave23)<=set(docs))
  self.assertEqual(sum(len(docs[k]['evidence']) for k in wave23),40)
  self.assertEqual(docs['anselmo-sacasas']['primary_source_url'],
                   'https://latinpop.fiu.edu/SECCION06S.pdf')

 def test_wave22_personal_histories_have_individual_provenance(self):
  docs=json.loads((OVERLAY/'source/jazz-editorial-dossiers.json').read_text('ascii'))
  artists=('abram-wilson','antoine-roney','caro-emerald','carol-welsman',
           'carsten-daerr','colin-hodgkinson','freddie-waits','gene-ludwig',
           'gerardo-velez','gerry-niewood','greg-gisbert','john-marshall',
           'marion-montgomery','michael-snow','mildred-bailey','mike-barone',
           'monica-zetterlund','monnette-sudler','ted-heath','teddy-edwards',
           'tom-browne','wallace-roney')
  self.assertEqual(len(artists),22)
  self.assertEqual(len(set(artists)),22)
  self.assertTrue(set(artists)<=set(docs))
  self.assertEqual(sum(len(docs[k]['evidence']) for k in artists),63)
  self.assertEqual(docs['gerardo-velez']['evidence'][0]['source_url'],
                   'https://www.bethelwoodscenter.org/news/detail/jimi-hendrix-50-years-of-peace-music')

 def test_generated_future_source_has_1190_exact_hints(self):
  header=(OVERLAY/'source/JazzNowPlaying.h').read_text('ascii')
  self.assertIn('static constexpr size_t kCuratedCount = 1190;',header)
  self.assertIn('Exact ICY matches only.',header)
  self.assertIn('static bool sameName',header)
  self.assertIn('return !*a && !*b',header)
  self.assertIn('Cassino Simpson',header)
  self.assertIn('Hagood Hardy',header)
  self.assertIn('Wallace Roney',header)
  self.assertIn('Monica Zetterlund',header)
  self.assertIn('Anselmo Sacasas',header)
  self.assertIn('Yolande Bavan',header)
  self.assertIn('Teo Macero',header)
  self.assertIn('George Bohanon',header)
  self.assertIn('Harry James',header)
  self.assertIn('Shunzo Ohno',header)
  self.assertIn('Sadao Watanabe',header)
  self.assertIn('Sammy Nestico',header)
  self.assertIn('Masha Bijlsma',header)
  self.assertIn('Gloria Coleman',header)
  self.assertIn('Nikki Iles',header)
  self.assertIn('Adele Girard',header)
  self.assertIn('Kenneth Kapstad',header)
  self.assertIn('Clarence Williams',header)
  self.assertIn('George Duke',header)
  self.assertIn('Orrin Evans',header)
  self.assertIn('Aage Tanggaard',header)
  self.assertIn('George E. Lee',header)
  self.assertIn('Simon Brehm',header)

 def test_frozen_public_firmware_source_was_not_replaced(self):
  self.assertTrue((ROOT/'firmware/SHA256-SOURCES.txt').is_file())
  version=(ROOT/'firmware/PocketJazzRadio/BuildVersion.h').read_text('ascii')
  self.assertIn('#define POCKETJAZZ_FIRMWARE_VERSION "v0.8.0"',version)
  self.assertNotEqual((ROOT/'firmware/data/jazz-encyclopedia.tsv').read_bytes(),
                      (OVERLAY/'sd/jazz-encyclopedia.tsv').read_bytes())

if __name__=='__main__':unittest.main()
