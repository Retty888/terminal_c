import { describe, expect, it } from "vitest";

import { parseTradeSignal, TradeSignalParseError } from "../src/trading/tradeSignalParser";

describe("parseTradeSignal", () => {
  it("parses a well structured signal", () => {
    const signal = parseTradeSignal(
      "Long BTC 0.75 entry 63000 stop 61500 take profit 64000 tp2 65000",
    );

    expect(signal.side).toBe("long");
    expect(signal.symbol).toBe("BTC");
    expect(signal.size).toBeCloseTo(0.75);
    expect(signal.entryPrice).toBeCloseTo(63000);
    expect(signal.stopLoss).toBeCloseTo(61500);
    expect(signal.takeProfits).toEqual([64000, 65000]);
    expect(signal.execution).toBe("limit");
  });

  it("detects market order when entry is missing", () => {
    const signal = parseTradeSignal(
      "short eth size=2 stop 3200 tp1 3000 tp2 2900 market",
    );
    expect(signal.side).toBe("short");
    expect(signal.symbol).toBe("ETH");
    expect(signal.execution).toBe("market");
    expect(signal.entryPrice).toBeUndefined();
    expect(signal.takeProfits).toEqual([3000, 2900]);
  });

  it("supports @ notation and comma decimals", () => {
    const signal = parseTradeSignal("Buy SOL @150.5 size 10 stoploss 140,2 tp 160 tp2 170");
    expect(signal.entryPrice).toBeCloseTo(150.5);
    expect(signal.size).toBeCloseTo(10);
    expect(signal.stopLoss).toBeCloseTo(140.2);
  });

  it("throws when stop loss missing", () => {
    expect(() => parseTradeSignal("Long BTC 1 take profit 66000")).toThrow(TradeSignalParseError);
  });
});

