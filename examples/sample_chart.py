"""
Example candlestick chart for BTC/USDT using Binance API.
Dependencies: requests, pandas, mplfinance
Install:  pip install requests pandas mplfinance
"""

import requests
import pandas as pd
import mplfinance as mpf

SYMBOL = "BTCUSDT"
INTERVAL = "1h"
LIMIT = 120


def load_candles():
    """Fetch candles from Binance and return a DataFrame."""
    url = (
        f"https://api.binance.com/api/v3/klines?symbol={SYMBOL}" \
        f"&interval={INTERVAL}&limit={LIMIT}"
    )
    resp = requests.get(url, timeout=10)
    resp.raise_for_status()
    data = resp.json()
    cols = [
        "open_time", "open", "high", "low", "close", "volume",
        "close_time", "qav", "num_trades",
        "taker_base", "taker_quote", "ignore",
    ]
    df = pd.DataFrame(data, columns=cols)
    df["open_time"] = pd.to_datetime(df["open_time"], unit="ms")
    df.set_index("open_time", inplace=True)
    df = df.astype(float)
    return df[["open", "high", "low", "close", "volume"]]


if __name__ == "__main__":
    candles = load_candles()
    mpf.plot(
        candles,
        type="candle",
        volume=True,
        style="yahoo",
        title=f"{SYMBOL} {INTERVAL} candles",
    )
