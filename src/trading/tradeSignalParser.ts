export type TradeSide = "long" | "short";
export type ExecutionType = "market" | "limit";

export interface TradeSignal {
  readonly side: TradeSide;
  readonly symbol: string;
  readonly rawSymbol: string;
  readonly size: number;
  readonly entryPrice?: number;
  readonly stopLoss?: number;
  readonly takeProfits: number[];
  readonly leverage?: number;
  readonly execution: ExecutionType;
  readonly text: string;
}

export class TradeSignalParseError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "TradeSignalParseError";
  }
}

const SYMBOL_SUFFIXES = ["PERPETUAL", "PERP", "USDT", "USD", "SPOT"] as const;
const SYMBOL_ALIASES: Record<string, string> = {
  XBT: "BTC",
};

const VALUE_KEYWORDS = [
  "size",
  "qty",
  "quantity",
  "amount",
  "volume",
  "entry",
  "price",
  "stop",
  "sl",
  "take",
  "tp",
  "target",
  "at",
];

const SPLIT_REGEX = /[\s,;]+/g;

const SIZE_PATTERNS = [
  /\b(?:size|qty|quantity|amount|volume)\b\s*(?:=|:)?\s*(-?\d+(?:[.,]\d+)?)/i,
];

const ENTRY_PATTERNS = [
  /\bentry\b\s*(?:=|:)?\s*(-?\d+(?:[.,]\d+)?)/i,
  /\bprice\b\s*(?:=|:)?\s*(-?\d+(?:[.,]\d+)?)/i,
  /(?:^|\s)@\s*(-?\d+(?:[.,]\d+)?)/i,
];

const STOP_PATTERNS = [
  /\b(?:stop(?:\s*loss)?|sl)\b\s*(?:=|:|@)?\s*(-?\d+(?:[.,]\d+)?)/i,
];

const LEVERAGE_PATTERNS = [
  /(\d+(?:[.,]\d+)?)\s*x\b/i,
  /\b(?:leverage|lev)\b\s*(?:=|:)?\s*(\d+(?:[.,]\d+)?)/i,
];

export function normalizeSymbol(raw: string): string {
  let symbol = raw.trim().toUpperCase();
  if (!symbol) {
    return "";
  }

  symbol = symbol.replace(/[^A-Z0-9]/g, "");
  for (const suffix of SYMBOL_SUFFIXES) {
    if (symbol.endsWith(suffix)) {
      symbol = symbol.slice(0, -suffix.length);
    }
  }
  if (symbol in SYMBOL_ALIASES) {
    symbol = SYMBOL_ALIASES[symbol];
  }
  return symbol;
}

function parseNumeric(value: string): number | undefined {
  const normalized = value.replace(/\s+/g, "").replace(",", ".");
  if (!/^[-+]?\d*(?:\.\d+)?$/.test(normalized)) {
    return undefined;
  }
  const result = Number.parseFloat(normalized);
  return Number.isFinite(result) ? result : undefined;
}

function cleanToken(token: string): string {
  return token.replace(/^[^A-Za-z0-9@]+|[^A-Za-z0-9@]+$/g, "");
}

function extractSingle(text: string, patterns: RegExp[]): number | undefined {
  for (const pattern of patterns) {
    pattern.lastIndex = 0;
    const match = pattern.exec(text);
    if (match) {
      const raw = match[match.length - 1];
      if (raw !== undefined) {
        const numeric = parseNumeric(raw);
        if (numeric !== undefined) {
          return numeric;
        }
      }
    }
  }
  return undefined;
}

function extractTakeProfits(text: string): number[] {
  const pattern = /\b(?:tp\d*|tp|take\s*profit|target)\b/gi;
  const values: number[] = [];
  const seen = new Set<string>();
  pattern.lastIndex = 0;
  let match: RegExpExecArray | null;
  while ((match = pattern.exec(text)) !== null) {
    const slice = text.slice(match.index + match[0].length);
    const numberMatch = /^\s*(?:=|:|@|at)?\s*(-?\d+(?:[.,]\d+)?)/i.exec(slice);
    if (!numberMatch) {
      continue;
    }
    const numeric = parseNumeric(numberMatch[1]);
    if (numeric === undefined) {
      continue;
    }
    const key = numeric.toString();
    if (!seen.has(key)) {
      values.push(numeric);
      seen.add(key);
    }
  }
  return values;
}

export function parseTradeSignal(text: string): TradeSignal {
  const trimmed = text.trim();
  if (!trimmed) {
    throw new TradeSignalParseError("Пустой текст сигнала");
  }

  const tokens = trimmed.split(SPLIT_REGEX).map((token) => cleanToken(token)).filter(Boolean);
  if (tokens.length === 0) {
    throw new TradeSignalParseError("Не удалось распознать сигнал");
  }

  let side: TradeSide | undefined;
  let sideIndex = -1;
  for (let i = 0; i < tokens.length; ++i) {
    const t = tokens[i].toLowerCase();
    if (t === "long" || t === "buy") {
      side = "long";
      sideIndex = i;
      break;
    }
    if (t === "short" || t === "sell") {
      side = "short";
      sideIndex = i;
      break;
    }
  }
  if (!side) {
    throw new TradeSignalParseError("Не удалось определить направление сделки (long/short)");
  }

  let rawSymbol = "";
  for (let i = sideIndex + 1; i < tokens.length; ++i) {
    const token = tokens[i];
    if (!token) {
      continue;
    }
    const lower = token.toLowerCase();
    if (VALUE_KEYWORDS.includes(lower)) {
      continue;
    }
    if (/^\d/.test(token)) {
      continue;
    }
    rawSymbol = token;
    break;
  }
  if (!rawSymbol) {
    throw new TradeSignalParseError("Не удалось определить торговый инструмент");
  }

  const normalizedSymbol = normalizeSymbol(rawSymbol);
  if (!normalizedSymbol) {
    throw new TradeSignalParseError(`Не удалось нормализовать символ "${rawSymbol}"`);
  }

  let size = extractSingle(trimmed, SIZE_PATTERNS);
  let entryPrice = extractSingle(trimmed, ENTRY_PATTERNS);
  if (size === undefined) {
    for (let i = sideIndex + 1; i < tokens.length; ++i) {
      const token = tokens[i];
      if (!token || token.toLowerCase() === "@") {
        continue;
      }
      if (/^@/.test(token)) {
        const candidate = parseNumeric(token.slice(1));
        if (candidate !== undefined) {
          if (entryPrice === undefined) {
            entryPrice = candidate;
          }
          continue;
        }
      }
      const numeric = parseNumeric(token);
      if (numeric !== undefined) {
        size = numeric;
        break;
      }
    }
  }
  if (size === undefined || size <= 0) {
    throw new TradeSignalParseError("Не удалось определить размер позиции");
  }

  let execution: ExecutionType = "limit";
  const hasMarket = /\bmarket\b/i.test(trimmed);
  const hasLimit = /\blimit\b/i.test(trimmed);
  if (hasMarket && !hasLimit) {
    execution = "market";
  } else if (!hasLimit && !hasMarket && entryPrice === undefined) {
    execution = "market";
  }

  const stopLoss = extractSingle(trimmed, STOP_PATTERNS);
  const takeProfits = extractTakeProfits(trimmed);

  const leverage = extractSingle(trimmed, LEVERAGE_PATTERNS);

  if (execution === "limit" && entryPrice === undefined) {
    execution = "market";
  }

  if (stopLoss === undefined) {
    throw new TradeSignalParseError("Не указан стоп-лосс");
  }
  if (takeProfits.length === 0) {
    throw new TradeSignalParseError("Не указаны цели тейк-профита");
  }

  return {
    side,
    symbol: normalizedSymbol,
    rawSymbol,
    size,
    entryPrice,
    stopLoss,
    takeProfits,
    leverage,
    execution,
    text: trimmed,
  };
}

