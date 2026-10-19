# Test fixtures

`klines_sample.json` is **synthetic**: hand-written numbers in the exact 12-field layout that
`GET /api/v3/klines` returns (SPEC section 1). It is not real market data. It exists only so the
parser test runs offline. Real data is downloaded by the program itself (menu option 1).
