  
    //AsyncResponseStream *response = request->beginResponseStream("text/html");
    //response->print("<!DOCTYPE html><html><head><title>Captive Portal</title></head><body>");
    const char index_html[] PROGMEM = R"rawliteral(
    <!DOCTYPE html><html><head><title>Schulgong</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <script>
    async function sync_from_browser() {
      const localized_timestamp = Math.floor(Date.now() / 1000) - (new Date().getTimezoneOffset())*60;
      const data = new FormData();
      data.append("set", localized_timestamp);
      const response = await fetch("/time", { method: "POST", body: data });
      window.location.reload();
    }
    function fill_sched_form(timestamp, sound, volume) {
      document.getElementById('timestamp').value = timestamp;
      document.getElementById('sound').value = sound;
      document.getElementById('volume').value = volume;
    }
    async function del_sched(timestamp) {
      if (!confirm("Really delete "+timestamp+"?")) return;
      const data = new FormData();
      data.append("delete", timestamp);
      const response = await fetch("/schedule", { method: "POST", body: data });
      window.location.reload();
    }
    async function del_sound(filename) {
      if (!confirm("Really delete "+filename+"?")) return;
      const data = new FormData();
      data.append("delete", filename);
      const response = await fetch("/sounds", { method: "POST", body: data });
      window.location.reload();
    }
    async function play_audio(timestamp) {
      const response = await fetch("/play?timestamp=" + timestamp);
    }
    </script>
    <style>
    input { width: 100%; }
    html, input, button, a {
      background: #383838;
      color: #e7e7e7;
    }
    input, button {
      background: #4c4a4a;
      border: 1px solid #e7e7e7;
      margin: 3px 0px;
    }
    td, th { padding: 0 3px; }
    table, th, td { border: 1px solid #e7e7e7; }
    table { border-collapse: collapse; }
    </style>
    </head><body>
    <p><center><h2>Schulgong</h2><br></center></p>
    <p>
      Time Status: %s<br>
      System Time: %s <button type="button" onclick="sync_from_browser();">Zeit vom Browser in Schulgong synchronisieren</button><br>
      DCF77 Time: %s<br>
      SD-Card Type: %s<br>
    <p>
    <form method="post" action="/schedule"><table border="1">
      <tr><th colspan="4">Zeitplan</th></tr>
      <tr><th>Time</th><th>Sound</th><th>Volume (0-21)</th><th>Action</th></tr>
      %s
      <tr>
        <td><input name="timestamp" id="timestamp"></td>
        <td><input name="sound" id="sound"></td>
        <td><input name="volume" id="volume"></td>
        <td><input type="submit" value="Save"></td>
    </table></form>
    <p>
    <table border="1">
      <tr><th colspan="3">Audiodateien</th></tr>
      <tr><th>Name</th><th>Gr&ouml;&szlig;e</th><th>Action</th></tr>
      %s
    </table>
    <p>
    <form method="post" action="/upload" enctype="multipart/form-data">
      <input type="file" name="data">
      <input type="submit">
    </form>
    </body></html>
    )rawliteral";
    //response->printf(index_html);
    /*
    response->print("<p>This is out captive portal front page.</p>");
    response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
    response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
  
    //response->print("</body></html>");
    request->send(response);
    */