import { createServer } from "node:http";
import { readFile, stat } from "node:fs/promises";
import { extname, join, normalize } from "node:path";

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".wasm": "application/wasm",
};

export async function startStaticServer(root, port) {
  const requests = [];
  const server = createServer(async (request, response) => {
    const url = new URL(request.url, `http://${request.headers.host}`);
    requests.push(url.pathname);
    let pathname = decodeURIComponent(url.pathname);
    for (const prefix of ["/demo/calculator/", "/nested/numos/"]) {
      if (pathname.startsWith(prefix)) pathname = pathname.slice(prefix.length);
    }
    pathname = pathname.replace(/^\/+/, "") || "index.html";
    const safe = normalize(pathname);
    if (safe.startsWith("..")) {
      response.writeHead(403).end("forbidden");
      return;
    }
    const path = join(root, safe);
    try {
      const [data, info] = await Promise.all([readFile(path), stat(path)]);
      const immutable = /\.[0-9a-f]{12}\./.test(pathname);
      response.writeHead(200, {
        "content-type": MIME[extname(path)] || "application/octet-stream",
        "content-length": info.size,
        "cache-control": immutable
          ? "public, max-age=31536000, immutable"
          : "no-cache",
      });
      response.end(data);
    } catch {
      response.writeHead(404, { "content-type": "text/plain" });
      response.end("not found");
    }
  });
  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(port, "127.0.0.1", resolve);
  });
  return {
    origin: `http://127.0.0.1:${port}`,
    requests,
    close: () => new Promise((resolve) => server.close(resolve)),
  };
}
