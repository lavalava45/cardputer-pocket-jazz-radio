"""Source wiring guard; does not simulate TLS or establish hardware latency."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HandshakeTimeoutContract(unittest.TestCase):
    def test_each_request_sets_sdk_handshake_seconds_before_begin(self):
        source = (ROOT / 'PocketJazzRadio/SecureICYSource.cpp').read_text(encoding='utf-8')
        request = source.split('bool SecureICYSource::startRequest(', 1)[1].split(
            'bool SecureICYSource::open(', 1)[0]
        setter = 'tls_.setHandshakeTimeout((connectTimeoutMs_ + 999u) / 1000u);'
        self.assertIn(setter, request)
        self.assertLess(request.index(setter), request.index('http_.begin('))
        self.assertIn('tls_.setCACert(caPem_);', request)
        self.assertNotIn('setInsecure(', source)


if __name__ == '__main__':
    unittest.main()
