let uri = "ws://127.0.0.1:8080";

document.getElementById('wstest').addEventListener('click', () => {
    const ws = new WebSocket(uri, "lws-minimal");
    ws.onopen = (evt) => {
        console.log('Connected');
    }
    
    ws.onclose = (evt) => {
        console.log('Disconnected');
    }

    ws.onmessage = (evt) => {
        console.log(evt);
    }

    ws.onerror = (evt) => {
        console.log(evt);
    }
})