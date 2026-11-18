#pragma once
/**
 * Single-page web UI (served from flash). No external resources: the phone is
 * connected to the ESP32's own Wi-Fi and has no internet.
 *
 * SECURITY NOTE: BLE device names/UUIDs are attacker-controlled (any nearby
 * device can advertise arbitrary text). The page therefore renders them ONLY
 * with textContent / DOM APIs, never innerHTML.
 */

#include <pgmspace.h>

static const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ARQUIS EV Diagnostic</title>
<style>
:root{--bg:#0e1116;--card:#171c24;--line:#2a3140;--fg:#e8edf4;--mut:#8b96a8;--acc:#33c3f0;--ok:#4cd07d;--bad:#ff6b6b}
@media (prefers-color-scheme:light){:root{--bg:#f4f6f9;--card:#fff;--line:#d5dbe5;--fg:#16202e;--mut:#5b6678;--acc:#0a78a8;--ok:#1a8f4a;--bad:#c0392b}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.4 system-ui,sans-serif;padding:16px;max-width:640px;margin-inline:auto}
h1{font-size:20px;margin:0 0 4px;color:var(--acc)}
h2{font-size:15px;margin:22px 0 8px;color:var(--mut);text-transform:uppercase;letter-spacing:.05em}
#st{font-size:13px;color:var(--mut)} #st.ok{color:var(--ok)} #st.bad{color:var(--bad)}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
button,select{font:inherit;padding:10px 14px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg)}
button{background:var(--acc);color:#06222e;border-color:var(--acc);font-weight:600}
button:disabled{opacity:.5}
button.ghost{background:var(--card);color:var(--fg);border-color:var(--line);font-weight:400}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px;margin:8px 0}
.name{font-weight:600;word-break:break-word}
.meta{font-size:13px;color:var(--mut);word-break:break-all;margin-top:2px}
.card button{margin-top:10px}
pre{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px;overflow:auto;font-size:12px;white-space:pre-wrap;word-break:break-all;min-height:80px}
.note{font-size:13px;color:var(--mut);border-left:3px solid var(--acc);padding-left:10px;margin:14px 0}
</style>
</head>
<body>
<h1>ARQUIS EV DIAGNOSTIC</h1>
<div id="st">Conectando…</div>
<p class="note">Solo lectura: esta herramienta observa. No escribe nada en el vehículo.</p>

<h2>Escaneo BLE</h2>
<div class="row">
  <select id="secs"><option value="5">5 s</option><option value="8" selected>8 s</option><option value="15">15 s</option></select>
  <button id="scan">Escanear</button>
  <span id="msg"></span>
</div>
<div id="list"></div>

<h2>Resultado GATT</h2>
<pre id="out">Pulsa «Explorar GATT» en un dispositivo conectable.</pre>
<div class="row">
  <button class="ghost" id="copy">Copiar</button>
  <button class="ghost" id="dl">Descargar .txt</button>
</div>
<p class="note">Los resultados contienen direcciones de dispositivos cercanos. Revísalos antes de compartirlos.</p>

<script>
(()=>{
const $=id=>document.getElementById(id);
let waitingScan=false,devices=[];
const fmt=s=>[Math.floor(s/3600),Math.floor(s/60)%60,s%60].map(n=>String(n).padStart(2,'0')).join(':');
async function j(path,opts){const r=await fetch(path,opts);if(!r.ok)throw new Error('HTTP '+r.status);return r}
function el(tag,cls,text){const e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e}

async function status(){
  try{
    const s=await (await j('/api/status')).json();
    $('st').textContent='FW '+s.fw+' · activo '+fmt(s.uptime_s)+' · heap '+Math.round(s.heap/1024)+' KB';
    $('st').className='ok';
    if(s.scanning){$('msg').textContent='Escaneando…';$('scan').disabled=true;}
    else{
      $('scan').disabled=false;
      if(waitingScan){waitingScan=false;await loadDevices();}
    }
  }catch(e){$('st').textContent='Sin conexión con el ESP32';$('st').className='bad';}
}
async function loadDevices(){
  const d=await (await j('/api/devices')).json();
  devices=d.devices;render();
  $('msg').textContent=devices.length+' dispositivo(s)';
}
function render(){
  const box=$('list');box.replaceChildren();
  devices.forEach((d,i)=>{
    const c=el('div','card');
    c.append(el('div','name',d.name||'(sin nombre)'));
    c.append(el('div','meta',d.addr+' · RSSI '+d.rssi+' dBm · '+(d.conn?'conectable':'no conectable')));
    if(d.svc)c.append(el('div','meta','Servicios: '+d.svc));
    if(d.mfr)c.append(el('div','meta','Fabricante: '+d.mfr));
    if(d.conn){
      const b=el('button',null,'Explorar GATT (solo lectura)');
      b.onclick=()=>gatt(i,b);
      c.append(b);
    }
    box.append(c);
  });
}
async function startScan(){
  $('scan').disabled=true;$('msg').textContent='Iniciando…';
  try{
    await j('/api/scan?secs='+$('secs').value,{method:'POST'});
    waitingScan=true;
  }catch(e){$('msg').textContent='No se pudo iniciar el escaneo';$('scan').disabled=false;}
}
async function gatt(i,btn){
  btn.disabled=true;
  $('out').textContent='Conectando y leyendo (puede tardar hasta ~30 s)…';
  $('out').scrollIntoView({behavior:'smooth'});
  const ctl=new AbortController();const t=setTimeout(()=>ctl.abort(),70000);
  try{
    const r=await j('/api/gatt?index='+i,{method:'POST',signal:ctl.signal});
    $('out').textContent=await r.text();
  }catch(e){$('out').textContent='Error: '+e.message;}
  clearTimeout(t);btn.disabled=false;
}
$('scan').onclick=startScan;
$('copy').onclick=()=>{
  const r=document.createRange();r.selectNodeContents($('out'));
  const s=getSelection();s.removeAllRanges();s.addRange(r);
  try{document.execCommand('copy');}catch(e){}
};
$('dl').onclick=()=>{
  const a=el('a');
  a.href=URL.createObjectURL(new Blob([$('out').textContent],{type:'text/plain'}));
  a.download='arquis-gatt-'+Date.now()+'.txt';
  document.body.append(a);a.click();a.remove();
};
setInterval(status,1500);status();
})();
</script>
</body>
</html>
)HTML";
