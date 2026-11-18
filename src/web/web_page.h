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
table{width:100%;border-collapse:collapse;font-size:14px}
td,th{padding:5px 6px;border-bottom:1px solid var(--line);text-align:left;vertical-align:top}
th{color:var(--mut);font-weight:500;width:46%}
.warn{color:var(--bad);font-weight:600}
.min{color:var(--bad)} .max{color:var(--ok)}
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

<h2>Reporte BMS (JBD)</h2>
<div id="bms" class="card">Pulsa «Reporte BMS JBD» en un dispositivo. Solo para baterías tuyas.</div>
<div class="row">
  <button class="ghost" id="bmsmd" disabled>Descargar reporte (.md)</button>
  <button class="ghost" id="bmsjson" disabled>Descargar .json</button>
</div>

<h2>Escucha (solo recibe)</h2>
<div class="row">
  <select id="lsecs"><option value="20">20 s</option><option value="30" selected>30 s</option><option value="60">60 s</option><option value="120">120 s</option></select>
  <button class="ghost" id="lstop" disabled>Detener</button>
  <button class="ghost" id="ldl" disabled>Descargar .txt</button>
</div>
<div id="lstat" class="note">Sin sesión. Pulsa «Escuchar» en un dispositivo propio. No se le envía ningún dato.</div>
<pre id="listen">—</pre>

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
      const L=el('button','ghost','Escuchar (solo recibe)');
      L.style.marginLeft='8px';
      L.onclick=()=>listen(i,L);
      c.append(L);
      const k=el('button','ghost','Reporte BMS JBD');
      k.style.marginLeft='8px';
      k.onclick=()=>bms(i,k);
      c.append(k);
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
const PROT={cell_overvoltage:'Sobretensión de celda',cell_undervoltage:'Subtensión de celda',pack_overvoltage:'Sobretensión del pack',pack_undervoltage:'Subtensión del pack',charge_overtemp:'Sobretemperatura en carga',charge_undertemp:'Subtemperatura en carga',discharge_overtemp:'Sobretemperatura en descarga',discharge_undertemp:'Subtemperatura en descarga',charge_overcurrent:'Sobrecorriente de carga',discharge_overcurrent:'Sobrecorriente de descarga',short_circuit:'Cortocircuito',frontend_ic_error:'Error del IC frontal',mos_software_lock:'Bloqueo MOS por software'};
let lastBms=null;
const f2=(n,d)=>(n).toFixed(d);
function rows(list){
  const t=el('table');
  list.forEach(([k,v,cls])=>{const r=el('tr');r.append(el('th',null,k));r.append(el('td',cls,v));t.append(r);});
  return t;
}
function bmsRows(r){
  const b=r.basic,out=[];
  out.push(['Nombre BLE',r.device_name||'(sin nombre)']);
  out.push(['Hardware (informado)',r.hardware_name||'(no informado)']);
  out.push(['Dirección BLE',r.address]);
  out.push(['Versión de software',b.sw_version]);
  out.push(['Fecha de fabricación',b.prod_year+'-'+String(b.prod_month).padStart(2,'0')+'-'+String(b.prod_day).padStart(2,'0')]);
  out.push(['Ciclos de carga',String(b.cycles)]);
  out.push(['Tensión del pack',f2(b.total_voltage_cv/100,2)+' V']);
  const a=b.current_ca/100;
  out.push(['Corriente',f2(a,2)+' A ('+(a>0?'cargando':a<0?'descargando':'en reposo')+')']);
  out.push(['Capacidad restante',f2(b.residual_cah/100,2)+' Ah de '+f2(b.nominal_cah/100,2)+' Ah nominales']);
  out.push(['Carga (RSOC del BMS)',b.rsoc+' %']);
  out.push(['MOSFET carga / descarga',(b.fet_charge?'ON':'off')+' / '+(b.fet_discharge?'ON':'off')]);
  out.push(['Celdas informadas',String(b.cell_count)]);
  b.temps_dc.forEach((t,i)=>out.push(['Temperatura '+(i+1),f2(t/10,1)+' °C']));
  out.push(['Protecciones activas',b.protections.length?b.protections.map(k=>PROT[k]||k).join(', '):'ninguna',b.protections.length?'warn':null]);
  return out;
}
function renderBms(r){
  const box=$('bms');box.replaceChildren();
  if(!r.ok){box.append(el('div','warn','No se pudo leer: '+(r.error||'sin respuesta')));return;}
  box.append(rows(bmsRows(r)));
  if(r.cells_mv){
    box.append(el('h2',null,'Celdas'));
    const s=r.cell_stats,t=el('table');
    r.cells_mv.forEach((mv,i)=>{
      const tr=el('tr');
      tr.append(el('th',null,'Celda '+(i+1)+(r.basic.balancing.includes(i+1)?' (balanceando)':'')));
      tr.append(el('td',(i+1===s.min_cell?'min':(i+1===s.max_cell?'max':null)),mv+' mV'));
      t.append(tr);
    });
    box.append(t);
    box.append(rows([['Mínima',s.min_mv+' mV (celda '+s.min_cell+')'],['Máxima',s.max_mv+' mV (celda '+s.max_cell+')'],['Promedio',s.avg_mv+' mV'],['Diferencia (máx−mín)',s.delta_mv+' mV']]));
  }else{box.append(el('p','note','El BMS no respondió a la petición de tensiones de celda.'));}
  box.append(el('p','note','Valores informados por el propio BMS. No sustituyen una prueba real de capacidad: el contador de ciclos puede haberse reprogramado.'));
}
function bmsMarkdown(r){
  const clean=s=>String(s||'').replace(/[`|\n\r]/g,' ');
  let m='# Reporte de BMS JBD\n\nFecha del reporte: '+new Date().toLocaleString()+'\n\n';
  m+='| Dato | Valor |\n|---|---|\n';
  bmsRows(r).forEach(([k,v])=>{m+='| '+clean(k)+' | '+clean(v)+' |\n';});
  if(r.cells_mv){
    m+='\n## Celdas\n\n| Celda | Tensión (mV) |\n|---|---|\n';
    r.cells_mv.forEach((mv,i)=>{m+='| '+(i+1)+(r.basic.balancing.includes(i+1)?' (balanceando)':'')+' | '+mv+' |\n';});
    const s=r.cell_stats;
    m+='\nMínima '+s.min_mv+' mV (celda '+s.min_cell+'), máxima '+s.max_mv+' mV (celda '+s.max_cell+'), promedio '+s.avg_mv+' mV, diferencia '+s.delta_mv+' mV.\n';
  }
  m+='\n> Valores informados por el BMS. No sustituyen una prueba real de capacidad; el contador de ciclos puede reprogramarse.\n';
  return m;
}
function download(name,text,type){
  const a=el('a');a.href=URL.createObjectURL(new Blob([text],{type}));a.download=name;
  document.body.append(a);a.click();a.remove();
}
async function bms(i,btn){
  if(!confirm('Se enviarán 3 peticiones estándar de LECTURA al BMS JBD (nunca se escriben parámetros). Úsalo solo con baterías tuyas. ¿Continuar?'))return;
  btn.disabled=true;
  $('bms').textContent='Conectando y leyendo el BMS (puede tardar ~15 s)…';
  $('bms').scrollIntoView({behavior:'smooth'});
  const ctl=new AbortController();const t=setTimeout(()=>ctl.abort(),70000);
  try{
    const r=await (await j('/api/bms?index='+i,{method:'POST',signal:ctl.signal})).json();
    lastBms=r;renderBms(r);
    $('bmsmd').disabled=$('bmsjson').disabled=!r.ok;
  }catch(e){$('bms').textContent='Error: '+e.message;}
  clearTimeout(t);btn.disabled=false;
}
$('bmsmd').onclick=()=>lastBms&&download('reporte-bms-'+Date.now()+'.md',bmsMarkdown(lastBms),'text/markdown');
$('bmsjson').onclick=()=>lastBms&&download('reporte-bms-'+Date.now()+'.json',JSON.stringify(lastBms,null,2),'application/json');
let lOff=0,lText='';
async function listen(i,btn){
  if(!confirm('Se conectará al dispositivo y se SUSCRIBIRÁ a sus notificaciones para registrar lo que transmite. No se le envía ningún dato. Úsalo solo con aparatos tuyos. ¿Continuar?'))return;
  btn.disabled=true;lOff=0;lText='';
  $('listen').textContent='Conectando…';$('ldl').disabled=true;
  $('listen').scrollIntoView({behavior:'smooth'});
  try{
    const r=await fetch('/api/listen?index='+i+'&secs='+$('lsecs').value,{method:'POST'});
    const o=await r.json();
    if(!r.ok){$('listen').textContent='No se pudo iniciar: '+(o.error||r.status);btn.disabled=false;return;}
  }catch(e){$('listen').textContent='Error: '+e.message;btn.disabled=false;return;}
  $('lstop').disabled=false;
  pollListen(btn);
}
async function pollListen(btn){
  try{
    const o=await (await j('/api/listen?offset='+lOff)).json();
    if(o.chunk){lText+=o.chunk;lOff+=o.chunk.length;$('listen').textContent=lText;}
    $('lstat').textContent=(o.active?('Escuchando… quedan '+o.secs_left+' s'):'Sesión terminada')+' · '+o.n+' notificaciones · '+o.bytes+' B'+(o.trunc?' · registro lleno (truncado)':'');
    if(o.active||o.chunk){setTimeout(()=>pollListen(btn),1000);}
    else{$('lstop').disabled=true;$('ldl').disabled=!lText;btn.disabled=false;}
  }catch(e){$('lstat').textContent='Sin conexión con el ESP32, reintentando…';setTimeout(()=>pollListen(btn),2000);}
}
$('lstop').onclick=async()=>{try{await j('/api/listen/stop',{method:'POST'});}catch(e){}};
$('ldl').onclick=()=>lText&&download('arquis-escucha-'+Date.now()+'.txt',lText,'text/plain');
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
