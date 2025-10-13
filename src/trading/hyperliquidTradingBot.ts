import {
  ExchangeClient,
  HttpTransport,
  InfoClient,
  type OrderParameters,
  type OrderResponseSuccess,
} from "@nktkas/hyperliquid";

type MetaAndAssetCtxsTuple = Awaited<ReturnType<InfoClient["metaAndAssetCtxs"]>>;
type AssetContexts = MetaAndAssetCtxsTuple[1];

import { normalizeSymbol, parseTradeSignal, type TradeSignal } from "./tradeSignalParser";

export interface HyperliquidBotOptions {
  readonly privateKey?: string;
  readonly isTestnet?: boolean;
  readonly slippageBps?: number;
  readonly metaRefreshIntervalMs?: number;
  readonly infoClient?: InfoClient;
  readonly exchangeClient?: ExchangeClient;
  readonly transport?: HttpTransport;
}

export interface ExecutionResult {
  readonly signal: TradeSignal;
  readonly payload: OrderParameters;
  readonly response: OrderResponseSuccess;
}

interface AssetMeta {
  readonly id: number;
  readonly name: string;
  readonly sizeDecimals: number;
}

interface CachedAssets {
  readonly timestamp: number;
  readonly assetsBySymbol: Map<string, AssetMeta>;
  readonly contexts: AssetContexts;
}

const DEFAULT_SLIPPAGE_BPS = 50; // 0.5%
const DEFAULT_CACHE_TTL_MS = 5_000;

function assertNever(value: never): never {
  throw new Error(`Unexpected value: ${value as unknown as string}`);
}

function oppositeSide(side: TradeSignal["side"]): boolean {
  if (side === "long") {
    return false;
  }
  if (side === "short") {
    return true;
  }
  return assertNever(side);
}

function entrySide(side: TradeSignal["side"]): boolean {
  if (side === "long") {
    return true;
  }
  if (side === "short") {
    return false;
  }
  return assertNever(side);
}

export class HyperliquidTradingBot {
  private readonly info: InfoClient;
  private readonly exchange: ExchangeClient;
  private readonly slippageBps: number;
  private readonly cacheTtlMs: number;

  private cache?: CachedAssets;

  constructor(options: HyperliquidBotOptions) {
    if (!options.exchangeClient && !options.privateKey) {
      throw new Error("Необходимо указать приватный ключ либо передать готовый ExchangeClient");
    }

    const transport = options.transport ?? new HttpTransport({ isTestnet: options.isTestnet ?? false });
    this.info = options.infoClient ?? new InfoClient({ transport });
    this.exchange = options.exchangeClient ?? new ExchangeClient({ wallet: options.privateKey as string, transport });
    this.slippageBps = options.slippageBps ?? DEFAULT_SLIPPAGE_BPS;
    this.cacheTtlMs = options.metaRefreshIntervalMs ?? DEFAULT_CACHE_TTL_MS;
  }

  async executeSignalText(text: string): Promise<ExecutionResult> {
    const signal = parseTradeSignal(text);
    return this.executeSignal(signal);
  }

  async executeSignal(signal: TradeSignal): Promise<ExecutionResult> {
    const cache = await this.ensureCache();
    const asset = this.resolveAsset(cache, signal.symbol);
    const { price, tif } = await this.resolveEntryPrice(signal, asset, cache);
    const payload = this.buildOrderPayload(signal, asset, price, tif);
    const response = await this.exchange.order(payload);
    return { signal, payload, response };
  }

  private async ensureCache(): Promise<CachedAssets> {
    const now = Date.now();
    if (this.cache && now - this.cache.timestamp < this.cacheTtlMs) {
      return this.cache;
    }

    const [meta, contexts] = await this.info.metaAndAssetCtxs();
    const assetsBySymbol = new Map<string, AssetMeta>();
    meta.universe.forEach((item, index) => {
      const baseSymbol = normalizeSymbol(item.name);
      if (baseSymbol) {
        assetsBySymbol.set(baseSymbol, {
          id: index,
          name: item.name,
          sizeDecimals: item.szDecimals,
        });
      }
    });

    this.cache = {
      timestamp: now,
      assetsBySymbol,
      contexts,
    };
    return this.cache;
  }

  private resolveAsset(cache: CachedAssets, symbol: string): AssetMeta {
    const key = normalizeSymbol(symbol);
    const asset = cache.assetsBySymbol.get(key);
    if (!asset) {
      throw new Error(`Инструмент "${symbol}" не поддерживается биржей Hyperliquid`);
    }
    return asset;
  }

  private async resolveEntryPrice(
    signal: TradeSignal,
    asset: AssetMeta,
    cache: CachedAssets,
  ): Promise<{ price: number; tif: "Gtc" | "Ioc" }> {
    if (signal.execution === "limit" && signal.entryPrice !== undefined) {
      return { price: signal.entryPrice, tif: "Gtc" };
    }

    const context = cache.contexts[asset.id];
    if (!context || context.midPx === null) {
      throw new Error("Не удалось получить рыночную цену. Укажите цену входа вручную");
    }

    const mid = Number(context.midPx);
    if (!Number.isFinite(mid) || mid <= 0) {
      throw new Error("Получена некорректная рыночная цена от Hyperliquid");
    }

    const slippage = this.slippageBps / 10_000;
    const factor = signal.side === "long" ? 1 + slippage : 1 - slippage;
    const price = mid * factor;
    return { price, tif: "Ioc" };
  }

  private buildOrderPayload(
    signal: TradeSignal,
    asset: AssetMeta,
    entryPrice: number,
    tif: "Gtc" | "Ioc",
  ): OrderParameters {
    const orders: OrderParameters["orders"] = [];

    const sizeFormatted = this.formatSize(signal.size, asset.sizeDecimals);
    const entry = {
      a: asset.id,
      b: entrySide(signal.side),
      p: this.formatPrice(entryPrice),
      s: sizeFormatted,
      r: false,
      t: {
        limit: {
          tif,
        },
      },
    } as const;
    orders.push(entry);

    const exitSide = oppositeSide(signal.side);
    const tpSizes = this.splitTakeProfitSizes(signal.size, signal.takeProfits.length, asset.sizeDecimals);
    signal.takeProfits.forEach((target, index) => {
      orders.push({
        a: asset.id,
        b: exitSide,
        p: this.formatPrice(target),
        s: tpSizes[index],
        r: true,
        t: {
          trigger: {
            isMarket: true,
            triggerPx: this.formatPrice(target),
            tpsl: "tp",
          },
        },
      });
    });

    if (signal.stopLoss === undefined) {
      throw new Error("Стоп-лосс не задан");
    }

    orders.push({
      a: asset.id,
      b: exitSide,
      p: this.formatPrice(signal.stopLoss),
      s: sizeFormatted,
      r: true,
      t: {
        trigger: {
          isMarket: true,
          triggerPx: this.formatPrice(signal.stopLoss),
          tpsl: "sl",
        },
      },
    });

    const grouping = signal.takeProfits.length > 0 ? "positionTpsl" : "na";
    return {
      orders,
      grouping,
    };
  }

  private formatSize(value: number, decimals: number): string {
    if (!(value > 0)) {
      throw new Error("Размер позиции должен быть положительным");
    }
    const factor = 10 ** decimals;
    const rounded = Math.round(value * factor) / factor;
    return rounded.toFixed(decimals).replace(/\.0+$/, "").replace(/(\.\d*?)0+$/, "$1");
  }

  private formatPrice(value: number): string {
    if (!(value > 0)) {
      throw new Error("Цена должна быть положительной");
    }
    const str = value.toFixed(6);
    return str.replace(/\.0+$/, "").replace(/(\.\d*?)0+$/, "$1");
  }

  private splitTakeProfitSizes(total: number, count: number, decimals: number): string[] {
    if (count <= 0) {
      return [];
    }
    const factor = 10 ** decimals;
    const totalUnits = Math.round(total * factor);
    const base = Math.floor(totalUnits / count);
    let remainder = totalUnits - base * count;
    const sizes: string[] = [];
    for (let i = 0; i < count; ++i) {
      let units = base;
      if (remainder > 0) {
        units += 1;
        remainder -= 1;
      }
      const value = units / factor;
      sizes.push(this.formatSize(value, decimals));
    }
    return sizes;
  }
}

