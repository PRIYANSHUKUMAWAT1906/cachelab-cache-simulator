const UINT64_MAX = (1n << 64n) - 1n;
const U64_MASK = UINT64_MAX;
const MT_N = 312;
const MT_M = 156;
const MT_MATRIX_A = 0xB5026F5AA96619E9n;
const MT_UPPER_MASK = 0xFFFFFFFF80000000n;
const MT_LOWER_MASK = 0x7FFFFFFFn;

class Mt19937_64 {
  constructor(seed) {
    this.state = new Array(MT_N);
    this.state[0] = seed;
    for (let i = 1; i < MT_N; ++i) {
      const previous = this.state[i - 1];
      this.state[i] = (6364136223846793005n * (previous ^ (previous >> 62n)) + BigInt(i)) & U64_MASK;
    }
    this.index = MT_N;
  }

  next() {
    if (this.index >= MT_N) this.twist();
    let value = this.state[this.index++];
    value ^= (value >> 29n) & 0x5555555555555555n;
    value ^= (value << 17n) & 0x71D67FFFEDA60000n;
    value ^= (value << 37n) & 0xFFF7EEE000000000n;
    value ^= value >> 43n;
    return value & U64_MASK;
  }

  twist() {
    for (let i = 0; i < MT_N; ++i) {
      const x = (this.state[i] & MT_UPPER_MASK) | (this.state[(i + 1) % MT_N] & MT_LOWER_MASK);
      let xa = x >> 1n;
      if ((x & 1n) !== 0n) xa ^= MT_MATRIX_A;
      this.state[i] = this.state[(i + MT_M) % MT_N] ^ xa;
    }
    this.index = 0;
  }
}

export function parseUnsigned(value, label, suffixes) {
  const match = value.trim().match(suffixes
    ? /^(\d+)\s*([kmg]?)$/i
    : /^(\d+)$/);
  if (!match) throw new Error(`${label} must be a non-negative whole number${suffixes ? " (optionally with K, M, or G)" : ""}.`);
  let result = BigInt(match[1]);
  if (suffixes && match[2]) {
    const power = { k: 1n, m: 2n, g: 3n }[match[2].toLowerCase()];
    result *= 1024n ** power;
  }
  if (result > UINT64_MAX) throw new Error(`${label} is too large (maximum is an unsigned 64-bit value).`);
  return result;
}

export function parseTrace(text) {
  const addresses = [];
  const lines = text.split(/\r?\n/);
  for (let i = 0; i < lines.length; ++i) {
    const token = lines[i].split("#", 1)[0].trim();
    if (!token) continue;
    if (!/^(?:0[xX][0-9a-fA-F]+|[0-9]+)$/.test(token)) {
      throw new Error(`Trace line ${i + 1}: invalid address "${token}". Use decimal or 0x-prefixed hex.`);
    }
    let address;
    try {
      address = BigInt(token);
    } catch {
      throw new Error(`Trace line ${i + 1}: invalid address "${token}".`);
    }
    if (address > UINT64_MAX) throw new Error(`Trace line ${i + 1}: address does not fit in 64 bits.`);
    addresses.push(address);
  }
  if (addresses.length === 0) throw new Error("The trace has no addresses to simulate.");
  return addresses;
}

class Cache {
  constructor(config, policy, seed) {
    this.config = config;
    this.policy = policy;
    this.tags = new Array(config.sets * config.ways).fill(0n);
    this.valid = new Uint8Array(config.sets * config.ways);
    this.stamps = new Float64Array(config.sets * config.ways);
    this.frequencies = new Float64Array(config.sets * config.ways);
    this.born = new Float64Array(config.sets * config.ways);
    this.clock = 0;
    this.rng = policy === "RANDOM" ? new Mt19937_64(seed) : null;
    this.accesses = 0;
    this.hits = 0;
    this.misses = 0;
    this.evictions = 0;
  }

  access(address) {
    const block = address / this.config.lineSize;
    const set = Number(block % BigInt(this.config.sets));
    const tag = block / BigInt(this.config.sets);
    const start = set * this.config.ways;
    ++this.accesses;
    for (let way = 0; way < this.config.ways; ++way) {
      const index = start + way;
      if (this.valid[index] && this.tags[index] === tag) {
        ++this.hits;
        if (this.policy === "LRU") this.stamps[index] = ++this.clock;
        if (this.policy === "LFU") ++this.frequencies[index];
        return;
      }
    }

    ++this.misses;
    let way = 0;
    while (way < this.config.ways && this.valid[start + way]) ++way;
    if (way === this.config.ways) {
      ++this.evictions;
      way = this.policy === "RANDOM"
        ? this.selectRandomVictim()
        : this.findVictim(start);
    }

    const index = start + way;
    this.valid[index] = 1;
    this.tags[index] = tag;
    if (this.policy === "LRU" || this.policy === "FIFO") this.stamps[index] = ++this.clock;
    if (this.policy === "LFU") {
      this.frequencies[index] = 1;
      this.born[index] = ++this.clock;
    }
  }

  selectRandomVictim() {
    const bound = BigInt(this.config.ways);
    const threshold = (1n << 64n) % bound;
    let value;
    do {
      value = this.rng.next();
    } while (value < threshold);
    return Number(value % bound);
  }

  findVictim(start) {
    let victim = 0;
    for (let way = 1; way < this.config.ways; ++way) {
      const index = start + way;
      const best = start + victim;
      if (this.policy === "LFU") {
        if (this.frequencies[index] < this.frequencies[best] ||
            (this.frequencies[index] === this.frequencies[best] && this.born[index] < this.born[best])) {
          victim = way;
        }
      } else if (this.stamps[index] < this.stamps[best]) {
        victim = way;
      }
    }
    return victim;
  }
}

export function* makeWorkloads(config, seed, count) {
  const rng = new Mt19937_64(seed);
  const line = config.lineSize;
  const lines = config.lines;
  const sets = BigInt(config.sets);
  const ways = BigInt(config.ways);
  const fit = lines / 2n > 0n ? lines / 2n : 1n;
  const over = lines + (lines / 4n > 0n ? lines / 4n : 1n);
  const hot = lines / 8n > 0n ? lines / 8n : 1n;

  yield ["sequential", function* () {
    for (let i = 0; i < count; ++i) yield (BigInt(i) * 8n) & U64_MASK;
  }];
  yield ["loop_fits", function* () {
    for (let i = 0; i < count; ++i) yield ((BigInt(i) % fit) * line) & U64_MASK;
  }];
  yield ["loop_exceeds", function* () {
    for (let i = 0; i < count; ++i) yield ((BigInt(i) % over) * line) & U64_MASK;
  }];
  yield ["random", function* () {
    for (let i = 0; i < count; ++i) yield ((rng.next() % (8n * lines)) * line) & U64_MASK;
  }];
  yield ["hotset", function* () {
    for (let i = 0; i < count; ++i) {
      if (rng.next() % 100n < 80n) yield ((rng.next() % hot) * line) & U64_MASK;
      else yield ((hot + rng.next() % (16n * lines)) * line) & U64_MASK;
    }
  }];
  yield ["conflict", function* () {
    for (let i = 0; i < count; ++i) yield (((BigInt(i) % (ways + 1n)) * sets) * line) & U64_MASK;
  }];
}

function yieldToBrowser() {
  return new Promise(resolve => setTimeout(resolve, 0));
}

export async function simulate(workloads, config, policies, seed, totalWork, onProgress) {
  const results = [];
  let completed = 0;
  let chunk = 0;
  const startedAt = performance.now();
  for (const [name, addresses] of workloads) {
    const caches = policies.map(policy => new Cache(config, policy, seed));
    const addressStream = typeof addresses === "function" ? addresses() : addresses;
    for (const address of addressStream) {
      for (const cache of caches) cache.access(address);
      ++completed;
      if (++chunk === 4000) {
        chunk = 0;
        onProgress(Math.floor(completed * 100 / totalWork));
        await yieldToBrowser();
      }
    }
    for (const cache of caches) results.push({
      workload: name,
      policy: cache.policy,
      accesses: cache.accesses,
      hits: cache.hits,
      misses: cache.misses,
      evictions: cache.evictions,
      hitRate: cache.accesses ? cache.hits / cache.accesses : 0
    });
  }
  return { results, elapsedMs: performance.now() - startedAt };
}
