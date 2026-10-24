import React, { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import {
  makeWorkloads,
  parseTrace,
  parseUnsigned,
  simulate
} from "./simulator.js";
import "./styles.css";

const UINT64_MAX = (1n << 64n) - 1n;
const POLICY_OPTIONS = ["LRU", "FIFO", "LFU", "RANDOM"];
const SAMPLE_TRACE = `# Sample trace
0x0000
0x0040
0x0080
0x0000
0x0040
0x00C0
0x0000
0x1000
0x2000
0x3000
0x4000
0x0000
64
128`;

function getConfig(values) {
  const capacity = parseUnsigned(values.capacity, "Capacity", true);
  const lineSize = parseUnsigned(values.lineSize, "Line size", false);
  const associativity = parseUnsigned(values.associativity, "Associativity", false);
  if (capacity === 0n || lineSize === 0n || associativity === 0n) {
    throw new Error("Capacity, line size, and associativity must be greater than zero.");
  }
  if ((lineSize & (lineSize - 1n)) !== 0n) throw new Error("Line size must be a power of two.");
  if (lineSize > capacity) throw new Error("Line size must not exceed capacity.");
  if (associativity > capacity / lineSize) throw new Error("Associativity exceeds the number of cache lines.");
  const lines = capacity / lineSize;
  if (capacity % lineSize !== 0n || lines % associativity !== 0n) {
    throw new Error("Capacity must be divisible by line size × associativity.");
  }
  const ways = Number(associativity);
  const sets = Number(lines / associativity);
  if (!Number.isSafeInteger(ways) || !Number.isSafeInteger(sets) || ways * sets > 2_000_000) {
    throw new Error("This browser GUI supports cache geometries up to 2,000,000 cache lines.");
  }
  return { capacity, lineSize, associativity, lines, ways, sets };
}

function csvEscape(value) {
  const text = String(value);
  return /[",\r\n]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text;
}

function buildCsv(results) {
  return [
    "workload,policy,accesses,hits,misses,evictions,hit_rate,miss_rate",
    ...results.map(result => [
      result.workload,
      result.policy,
      result.accesses,
      result.hits,
      result.misses,
      result.evictions,
      result.hitRate.toFixed(6),
      (1 - result.hitRate).toFixed(6)
    ].map(csvEscape).join(","))
  ].join("\r\n");
}

function App() {
  const [values, setValues] = useState({
    capacity: "16K",
    lineSize: "64",
    associativity: "4",
    accesses: "200000",
    seed: "42"
  });
  const [policies, setPolicies] = useState(POLICY_OPTIONS);
  const [mode, setMode] = useState("trace");
  const [trace, setTrace] = useState(SAMPLE_TRACE);
  const [results, setResults] = useState([]);
  const [summary, setSummary] = useState("");
  const [status, setStatus] = useState("");
  const [error, setError] = useState("");
  const [running, setRunning] = useState(false);
  const [downloadUrl, setDownloadUrl] = useState("");
  const [chatMessages, setChatMessages] = useState([]);
  const [chatInput, setChatInput] = useState("");
  const [chatPending, setChatPending] = useState(false);
  const [chatOpen, setChatOpen] = useState(false);

  useEffect(() => {
    if (!downloadUrl) return undefined;
    return () => URL.revokeObjectURL(downloadUrl);
  }, [downloadUrl]);

  function updateValue(event) {
    const { name, value } = event.target;
    setValues(current => ({ ...current, [name]: value }));
  }

  function togglePolicy(policy) {
    setPolicies(current => current.includes(policy)
      ? current.filter(item => item !== policy)
      : [...current, policy]);
  }

  async function handleFile(event) {
    const file = event.target.files?.[0];
    if (!file) return;
    try {
      setTrace(await file.text());
      setError("");
      setStatus(`Loaded ${file.name}`);
    } catch (cause) {
      setError(`Could not read file: ${cause instanceof Error ? cause.message : String(cause)}`);
    }
  }

  async function runSimulation() {
    setRunning(true);
    setError("");
    setStatus("Preparing simulation…");
    setResults([]);
    setSummary("");
    setDownloadUrl("");

    try {
      const config = getConfig(values);
      if (policies.length === 0) throw new Error("Select at least one replacement policy.");

      let workloads;
      let totalWork;
      let seed = 42n;
      if (mode === "benchmark") {
        const count = Number(parseUnsigned(values.accesses, "Accesses", false));
        if (!Number.isSafeInteger(count) || count < 1 || count > 2_000_000) {
          throw new Error("Accesses must be between 1 and 2,000,000 per workload in the browser GUI.");
        }
        seed = parseUnsigned(values.seed, "Seed", false);
        workloads = makeWorkloads(config, seed, count);
        totalWork = count * 6;
      } else {
        const addresses = parseTrace(trace);
        workloads = [["trace", addresses]];
        totalWork = addresses.length;
      }

      const { results: simulationResults, elapsedMs } = await simulate(
        workloads,
        config,
        policies,
        seed,
        totalWork,
        percent => setStatus(`Simulating… ${percent}%`)
      );
      setResults(simulationResults);
      setSummary(
        `${config.capacity.toLocaleString()} B cache · ${config.lineSize} B lines · ` +
        `${config.associativity}-way · ${config.sets.toLocaleString()} sets · ` +
        `completed in ${(elapsedMs / 1000).toFixed(2)} s`
      );
      setDownloadUrl(URL.createObjectURL(new Blob([buildCsv(simulationResults)], {
        type: "text/csv;charset=utf-8"
      })));
      setStatus(`Finished: ${simulationResults.length} result${simulationResults.length === 1 ? "" : "s"}.`);
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
      setStatus("");
    } finally {
      setRunning(false);
    }
  }

  async function sendChatMessage(event) {
    event.preventDefault();
    const text = chatInput.trim();
    if (!text || chatPending) return;

    const conversation = [
      ...chatMessages.filter(message => message.role !== "notice"),
      { role: "user", text }
    ].slice(-12);
    setChatMessages(conversation);
    setChatInput("");
    setChatPending(true);

    try {
      const response = await fetch("/api/chat", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ messages: conversation })
      });
      const result = await response.json();
      if (!response.ok) throw new Error(result.error || "The assistant request failed.");
      if (typeof result.reply !== "string" || !result.reply.trim()) {
        throw new Error("The assistant returned an empty response.");
      }
      setChatMessages(current => [...current, { role: "model", text: result.reply }]);
    } catch (cause) {
      console.error("CacheLab AI request failed:", cause);
      setChatMessages(current => [...current, {
        role: "notice",
        text: "I’m having trouble responding right now because of a temporary connection or service issue. Please come back and try again in a little while."
      }]);
    } finally {
      setChatPending(false);
    }
  }

  return (
    <main>
      <header>
        <h1>CacheLab Simulator</h1>
        <p>Explore how cache size, associativity, and replacement policies affect hits and misses.</p>
      </header>

      <section className="panel" aria-labelledby="settings-title">
        <h2 id="settings-title">Cache settings</h2>
        <div className="grid">
          <label>Capacity
            <input name="capacity" value={values.capacity} onChange={updateValue} autoComplete="off" />
          </label>
          <label>Cache line size (bytes)
            <input name="lineSize" type="number" min="1" step="1" value={values.lineSize} onChange={updateValue} />
          </label>
          <label>Associativity (ways)
            <input name="associativity" type="number" min="1" step="1" value={values.associativity} onChange={updateValue} />
          </label>
        </div>
        <fieldset>
          <legend>Replacement policies</legend>
          <div className="policies">
            {POLICY_OPTIONS.map(policy => (
              <label key={policy}>
                <input
                  type="checkbox"
                  checked={policies.includes(policy)}
                  onChange={() => togglePolicy(policy)}
                />
                {policy === "RANDOM" ? "Random" : policy}
              </label>
            ))}
          </div>
        </fieldset>
      </section>

      <section className="panel" aria-labelledby="input-title">
        <h2 id="input-title">Input</h2>
        <label>Simulation type
          <select value={mode} onChange={event => { setMode(event.target.value); setError(""); }}>
            <option value="trace">Address trace</option>
            <option value="benchmark">Built-in benchmark workloads</option>
          </select>
        </label>
        {mode === "trace" ? (
          <div>
            <p>Enter one decimal or 0x-prefixed hexadecimal byte address per line. Blank lines and # comments are ignored. You can also select a trace file.</p>
            <label htmlFor="trace-file">Load a trace file</label>
            <input id="trace-file" type="file" accept=".trace,.txt,.log" onChange={handleFile} />
            <label htmlFor="trace">Trace addresses</label>
            <textarea id="trace" spellCheck="false" value={trace} onChange={event => setTrace(event.target.value)} />
          </div>
        ) : (
          <div className="grid benchmark-input">
            <label>Accesses per workload
              <input name="accesses" type="number" min="1" step="1" value={values.accesses} onChange={updateValue} />
            </label>
            <label>Random seed
              <input name="seed" type="number" min="0" step="1" value={values.seed} onChange={updateValue} />
            </label>
          </div>
        )}
        <div className="actions">
          <button type="button" onClick={runSimulation} disabled={running}>Run simulation</button>
          {downloadUrl && <a className="download" href={downloadUrl} download="cachelab-results.csv">Download CSV</a>}
          <span className="status" role="status" aria-live="polite">{status}</span>
        </div>
        <p className="error" role="alert">{error}</p>
      </section>

      {results.length > 0 && (
        <section className="panel" aria-labelledby="results-title">
          <h2 id="results-title">Results</h2>
          <p>{summary}</p>
          <div className="table-wrap">
            <table>
              <thead>
                <tr>
                  <th>Workload</th>
                  <th>Policy</th>
                  <th>Accesses</th>
                  <th>Hits</th>
                  <th>Misses</th>
                  <th>Evictions</th>
                  <th>Hit rate</th>
                </tr>
              </thead>
              <tbody>
                {results.map((result, index) => (
                  <tr key={`${result.workload}-${result.policy}-${index}`}>
                    <td>{result.workload}</td>
                    <td>{result.policy}</td>
                    <td>{result.accesses.toLocaleString()}</td>
                    <td>{result.hits.toLocaleString()}</td>
                    <td>{result.misses.toLocaleString()}</td>
                    <td>{result.evictions.toLocaleString()}</td>
                    <td className="bar-cell">
                      {(result.hitRate * 100).toFixed(2)}%
                      <div className="bar"><span style={{ width: `${result.hitRate * 100}%` }} /></div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>
      )}

      <div className="chat-widget">
        {chatOpen && (
          <section id="assistant-panel" className="chat-panel" aria-labelledby="assistant-title">
            <div className="chat-header">
              <div>
                <h2 id="assistant-title">CacheLab AI</h2>
                <p>Ask about caches or the simulator</p>
              </div>
              <button
                className="chat-close"
                type="button"
                onClick={() => setChatOpen(false)}
                aria-label="Close chat"
              >
                ×
              </button>
            </div>
            <div className="chat-log" aria-live="polite" aria-label="Conversation">
              {chatMessages.length === 0 && (
                <p className="chat-empty">Try asking: “What does associativity mean?”</p>
              )}
              {chatMessages.map((message, index) => (
                <div className={`chat-message ${message.role}`} key={`${index}-${message.role}`}>
                  <strong>{message.role === "user" ? "You" : message.role === "notice" ? "CacheLab AI" : "Assistant"}</strong>
                  <p>{message.text}</p>
                </div>
              ))}
              {chatPending && <p className="status" role="status">Assistant is thinking…</p>}
            </div>
            <form className="chat-form" onSubmit={sendChatMessage}>
              <label className="visually-hidden" htmlFor="chat-input">Your question</label>
              <textarea
                id="chat-input"
                value={chatInput}
                onChange={event => setChatInput(event.target.value)}
                maxLength={4000}
                placeholder="Ask a question…"
                rows={2}
                disabled={chatPending}
              />
              <button type="submit" disabled={chatPending || !chatInput.trim()}>Send</button>
            </form>
          </section>
        )}
        <button
          className="chat-launcher"
          type="button"
          onClick={() => setChatOpen(open => !open)}
          aria-expanded={chatOpen}
          aria-controls="assistant-panel"
          aria-label={chatOpen ? "Close CacheLab AI chat" : "Open CacheLab AI chat"}
        >
          <span aria-hidden="true">{chatOpen ? "×" : "✦"}</span>
          <span>{chatOpen ? "Close chat" : "Ask AI"}</span>
        </button>
      </div>
    </main>
  );
}

createRoot(document.getElementById("root")).render(<App />);
