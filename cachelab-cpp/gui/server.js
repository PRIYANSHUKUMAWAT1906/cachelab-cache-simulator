import { createServer } from "node:http";
import { ApiError, GoogleGenAI } from "@google/genai";

const host = "127.0.0.1";
const port = Number(process.env.API_PORT || 3001);
const maxBodyBytes = 64 * 1024;
const maxMessages = 12;
const maxMessageLength = 4000;
const primaryModel = "gemini-3.8-flash";
const fallbackModel = "gemini-3.6-flash";

function respond(response, status, body) {
  response.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "cache-control": "no-store"
  });
  response.end(JSON.stringify(body));
}

async function readJson(request) {
  let body = "";
  for await (const chunk of request) {
    body += chunk;
    if (Buffer.byteLength(body) > maxBodyBytes) {
      const error = new Error("Request body is too large.");
      error.status = 413;
      throw error;
    }
  }

  try {
    return JSON.parse(body);
  } catch {
    const error = new Error("Request body must be valid JSON.");
    error.status = 400;
    throw error;
  }
}

function validateMessages(messages) {
  if (!Array.isArray(messages) || messages.length === 0 || messages.length > maxMessages) {
    throw Object.assign(new Error(`Send between 1 and ${maxMessages} messages.`), { status: 400 });
  }

  const valid = messages.every(message =>
    message &&
    (message.role === "user" || message.role === "model") &&
    typeof message.text === "string" &&
    message.text.trim().length > 0 &&
    message.text.length <= maxMessageLength
  );
  if (!valid || messages[messages.length - 1].role !== "user") {
    throw Object.assign(new Error("Messages must contain valid user and assistant text, ending with a user message."), {
      status: 400
    });
  }
}

async function createChatInteraction(ai, input) {
  const request = {
    input,
    system_instruction:
      "You are the CacheLab assistant. Help with cache concepts and this cache simulator. Keep answers short, clear, and practical (usually 2-4 sentences).",
    generation_config: { max_output_tokens: 300 },
    store: false
  };

  try {
    return await ai.interactions.create({ ...request, model: primaryModel }, { timeout_ms: 30_000 });
  } catch (error) {
    if (!(error instanceof ApiError) || error.status !== 503) throw error;
    console.warn(`${primaryModel} is overloaded; retrying with ${fallbackModel}.`);
    return ai.interactions.create({ ...request, model: fallbackModel }, { timeout_ms: 30_000 });
  }
}

async function handleChat(request, response) {
  if (request.method !== "POST" || request.url !== "/api/chat") {
    respond(response, 404, { error: "Not found." });
    return;
  }

  if (!process.env.GEMINI_API_KEY) {
    respond(response, 503, { error: "Set GEMINI_API_KEY in the environment to enable the AI assistant." });
    return;
  }

  try {
    const body = await readJson(request);
    const messages = body?.messages;
    validateMessages(messages);

    const ai = new GoogleGenAI({ apiKey: process.env.GEMINI_API_KEY });
    const input = messages.map(message =>
        `${message.role === "model" ? "Assistant" : "User"}: ${message.text}`
      ).join("\n\n");
    const result = await createChatInteraction(ai, input);

    const reply = result.output_text?.trim();
    if (!reply) {
      respond(response, 502, { error: "Gemini returned no text. Try rephrasing your question." });
      return;
    }
    respond(response, 200, { reply });
  } catch (error) {
    if (response.headersSent) return;
    if (error.name === "TimeoutError" || error.name === "AbortError") {
      respond(response, 504, { error: "Gemini did not respond in time. Please try again." });
      return;
    }
    if (error instanceof ApiError) {
      const message = error.status === 503
        ? `Gemini is temporarily overloaded. Please try again shortly. Details: ${error.message.slice(0, 220)}`
        : `Gemini API error: ${error.message.slice(0, 300)}`;
      respond(response, error.status === 503 ? 503 : 502, { error: message });
      return;
    }
    respond(response, error.status || 502, {
      error: error.status ? error.message : "Could not connect to Gemini. Check the server and try again."
    });
  }
}

const server = createServer((request, response) => {
  void handleChat(request, response);
});

server.listen(port, host, () => {
  console.log(`CacheLab Gemini API listening at http://${host}:${port}`);
});
