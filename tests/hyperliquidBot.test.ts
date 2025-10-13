import { describe, expect, it } from "vitest";

import type {
  ExchangeClient,
  InfoClient,
  OrderParameters,
  OrderResponseSuccess,
} from "@nktkas/hyperliquid";
import { HyperliquidTradingBot } from "../src/trading/hyperliquidTradingBot";

type MetaAndAssetCtxsTuple = Awaited<ReturnType<InfoClient["metaAndAssetCtxs"]>>;
type MetaResponse = MetaAndAssetCtxsTuple[0];
type AssetContexts = MetaAndAssetCtxsTuple[1];

const baseMeta: MetaResponse = {
  universe: [
    {
      name: "BTC",
      szDecimals: 3,
      maxLeverage: 100,
      marginTableId: 0,
    },
  ],
  marginTables: [
    [
      0,
      {
        description: "default",
        marginTiers: [],
      },
    ],
  ],
};

const baseContexts: AssetContexts = [
  {
    prevDayPx: "60000",
    dayNtlVlm: "0",
    markPx: "60500",
    midPx: "60500",
    funding: "0",
    openInterest: "0",
    premium: "0",
    oraclePx: "60500",
    impactPxs: null,
    dayBaseVlm: "0",
  },
];

class FakeInfoClient {
  public calls = 0;
  constructor(private readonly meta: MetaResponse, private readonly contexts: AssetContexts) {}

  async metaAndAssetCtxs(): Promise<MetaAndAssetCtxsTuple> {
    this.calls += 1;
    return [this.meta, this.contexts] as MetaAndAssetCtxsTuple;
  }
}

class FakeExchangeClient {
  public orders: OrderParameters[] = [];

  async order(payload: OrderParameters): Promise<OrderResponseSuccess> {
    this.orders.push(payload);
    return {
      status: "ok",
      data: {
        statuses: payload.orders.map(() => ({ status: "fulfilled" })),
      },
    } as OrderResponseSuccess;
  }
}

describe("HyperliquidTradingBot", () => {
  it("creates market entry with grouped TP/SL", async () => {
    const info = new FakeInfoClient(baseMeta, baseContexts);
    const exchange = new FakeExchangeClient();
    const bot = new HyperliquidTradingBot({
      infoClient: info as unknown as InfoClient,
      exchangeClient: exchange as unknown as ExchangeClient,
      slippageBps: 100,
      metaRefreshIntervalMs: 100_000,
    });

    const result = await bot.executeSignalText("Long BTC 2 stop 58000 tp1 62000 tp2 63000 market");

    expect(info.calls).toBe(1);
    expect(result.payload.grouping).toBe("positionTpsl");
    expect(exchange.orders).toHaveLength(1);
    const [entry, tp1, tp2, stop] = exchange.orders[0].orders;
    expect(entry.t?.limit?.tif).toBe("Ioc");
    expect(Number(entry.p)).toBeCloseTo(60500 * 1.01, 3);
    expect(tp1.t?.trigger?.tpsl).toBe("tp");
    expect(tp1.r).toBe(true);
    expect(tp1.s).toBe("1");
    expect(tp2.s).toBe("1");
    expect(stop.t?.trigger?.tpsl).toBe("sl");
    expect(stop.s).toBe("2");
  });

  it("respects explicit limit price", async () => {
    const info = new FakeInfoClient(baseMeta, baseContexts);
    const exchange = new FakeExchangeClient();
    const bot = new HyperliquidTradingBot({
      infoClient: info as unknown as InfoClient,
      exchangeClient: exchange as unknown as ExchangeClient,
    });

    await bot.executeSignalText("Short BTC size 1 entry 60000 stop 62000 tp 58000");
    const [entry, tp, stop] = exchange.orders[0].orders;
    expect(entry.t?.limit?.tif).toBe("Gtc");
    expect(entry.b).toBe(false);
    expect(entry.p).toBe("60000");
    expect(tp.t?.trigger?.tpsl).toBe("tp");
    expect(stop.t?.trigger?.tpsl).toBe("sl");
  });
});

