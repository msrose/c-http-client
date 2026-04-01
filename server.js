const http = require('http')

const PORT = process.env.PORT ?? 3002

const server = http.createServer((req, res) => {
  console.log(`Got a req: ${req.method} ${req.url}`);
  let body = ""
  req.on("data", (data) => {
    const str = data.toString()
    console.log("Got a chunk:", str)
    body += str
  })
  req.on("end", () => {
    res.setHeader("content-type", "application/json");
    res.write(JSON.stringify({
      method: req.method,
      url: req.url,
      body: body.repeat(3),
      protocol: req.protocol
    }))
    res.end()
  })
})

server.on("connection", (socket) => {
  console.log(
    "Got a socket connection",
    `${socket.remoteAddress}:${socket.remotePort} (${socket.remoteFamily})`
  )
})

server.listen(PORT, () => {
  console.log(`Listening on port ${PORT}`)
});
