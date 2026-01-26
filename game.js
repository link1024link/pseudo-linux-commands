/* ================= CONFIG ================= */
const SCREEN_WIDTH = 60;
const SCREEN_HEIGHT = 20;
const FPS = 10;

const MAX_BULLETS = 12;
const MAX_ENEMY_BULLETS = 20;
const MAX_ENEMIES = 4;

const CONFIG = {
  playerMaxHP: 5,   // ← 好きに変更
  useImage: true
};

/* ================= CANVAS ================= */
const canvas = document.getElementById("gameCanvas");
const ctx = canvas.getContext("2d");
const cellW = canvas.width / SCREEN_WIDTH;
const cellH = canvas.height / SCREEN_HEIGHT;
ctx.font = `${cellH}px monospace`;
ctx.textBaseline = "top";

/* ================= SPRITE ================= */
class Sprite {
  constructor(src){
    this.img = new Image();
    this.loaded = false;
    this.img.onload = ()=>this.loaded=true;
    this.img.onerror = ()=>this.loaded=false;
    this.img.src = src;
  }
}

/* ================= STRUCTS ================= */
class GameObject {
  constructor(x=0,y=0,symbol=' '){
    this.x=x;
    this.y=y;
    this.symbol=symbol;
    this.is_active=0;

    this.scale = 30.0; // ← 追加
  }
}


class Enemy {
  constructor(x,y){
    this.x=x; this.y=y;
    this.hp=1;
    this.is_active=1;
    this.dir=1;
    this.scale = 4.5; // 敵は大きめ
  }
}

class Boss {
  constructor(){
    this.x=SCREEN_WIDTH>>1;
    this.y=1;
    this.hp=50;
    this.max_hp=50;
    this.is_active=0;
    this.dir=1;
    this.scale = 6.0; // ボスは巨大
  }
}


/* ================= SPRITES ================= */
const sprites = {
  player: new Sprite("Designer (2).png"),
  enemy: new Sprite("Designer.png"),
  boss: new Sprite("Designer (1).png"), 
  bullet: new Sprite("Designer.png"),
  ebullet: new Sprite("Designer.png")
};

/* ================= GAME DATA ================= */
const player = new GameObject();
player.max_hp = CONFIG.playerMaxHP;
player.hp = player.max_hp;

const bullets = Array.from({length:MAX_BULLETS},()=>new GameObject(0,0,'|'));
const enemyBullets = Array.from({length:MAX_ENEMY_BULLETS},()=>new GameObject(0,0,'!'));

let enemies = [];
let boss = new Boss();
let score = 0;
let keys = {};

/* ================= INIT ================= */
function initGame(){
  player.x = SCREEN_WIDTH>>1;
  player.y = SCREEN_HEIGHT-2;
  player.hp = player.max_hp;
  player.is_active = 1;

  bullets.forEach(b=>b.is_active=0);
  enemyBullets.forEach(b=>b.is_active=0);

  enemies = [];
  for(let i=0;i<MAX_ENEMIES;i++){
    enemies.push(new Enemy(Math.random()*SCREEN_WIDTH|0,2));
  }

  boss.is_active = 0;
  score = 0;
}

/* ================= INPUT ================= */
window.addEventListener("keydown",e=>{
  keys[e.key.toLowerCase()]=true;
  if(e.key===" ") e.preventDefault();
});
window.addEventListener("keyup",e=>{
  keys[e.key.toLowerCase()]=false;
});

function handleInput(){
  if(keys["a"] && player.x>0) player.x--;
  if(keys["d"] && player.x<SCREEN_WIDTH-1) player.x++;

  if(keys[" "]){
    for(let b of bullets){
      if(!b.is_active){
        b.x=player.x;
        b.y=player.y-1;
        b.is_active=1;
        break;
      }
    }
    keys[" "] = false;
  }
}

/* ================= UPDATE ================= */
function updateBullets(){
  for(let b of bullets){
    if(!b.is_active) continue;
    b.y--;
    if(b.y<0) b.is_active=0;

    for(let e of enemies){
      if(e.is_active && b.x===e.x && b.y===e.y){
        b.is_active=0;
        e.is_active=0;
        score+=100;
      }
    }

    if(boss.is_active && b.x===boss.x && b.y===boss.y){
      b.is_active=0;
      boss.hp--;
      if(boss.hp<=0){
        boss.is_active=0;
        score+=1000;
      }
    }
  }
}

function updateEnemies(){
  enemies.forEach(e=>{
    if(!e.is_active) return;
    e.x+=e.dir;
    if(e.x<=0||e.x>=SCREEN_WIDTH-1) e.dir*=-1;
  });
}

function updateBoss(){
  if(!boss.is_active) return;
  boss.x+=boss.dir;
  if(boss.x<=0||boss.x>=SCREEN_WIDTH-1) boss.dir*=-1;
}

function enemyFire(){
  enemies.forEach(e=>{
    if(e.is_active && Math.random()*20<1){
      for(let b of enemyBullets){
        if(!b.is_active){
          b.x=e.x; b.y=e.y+1;
          b.is_active=1;
          break;
        }
      }
    }
  });

  if(boss.is_active && Math.random()*5<1){
    for(let b of enemyBullets){
      if(!b.is_active){
        b.x=boss.x; b.y=boss.y+1;
        b.is_active=1;
        break;
      }
    }
  }
}

function updateEnemyBullets(){
  for(let b of enemyBullets){
    if(!b.is_active) continue;
    b.y++;
    if(b.y>=SCREEN_HEIGHT) b.is_active=0;

    if(b.x===player.x && b.y===player.y){
      b.is_active=0;
      player.hp--;
      if(player.hp<=0){
        alert("GAME OVER\nScore:"+score);
        initGame();
      }
    }
  }
}

function checkBossSpawn(){
  if(score>=800 && !boss.is_active){
    boss.is_active=1;
    boss.hp=boss.max_hp;
  }
}

/* ================= DRAW ================= */
function drawObj(o,sp){
  const px=o.x*cellW, py=o.y*cellH;
  if(CONFIG.useImage && sp.loaded){
    ctx.drawImage(sp.img,px,py,cellW,cellH);
  }else{
    ctx.fillText(o.symbol,px,py);
  }
}

function drawHPBar(x,y,w,hp,max){
  ctx.fillStyle="#400";
  ctx.fillRect(x,y,w,6);
  ctx.fillStyle="#f00";
  ctx.fillRect(x,y,w*(hp/max),6);
}

/* ================= RENDER ================= */
function render(){
  ctx.fillStyle="#001020";
  ctx.fillRect(0,0,canvas.width,canvas.height);

  ctx.fillStyle="#f55";
  enemies.forEach(e=>e.is_active && drawObj(e,sprites.enemy));

  if(boss.is_active){
    ctx.fillStyle="#f00";
    drawObj(boss,sprites.boss);
    drawHPBar(canvas.width/2-150,4,300,boss.hp,boss.max_hp);
  }

  ctx.fillStyle="#5f5";
  drawObj(player,sprites.player);

  ctx.fillStyle="#ff5";
  bullets.forEach(b=>b.is_active && drawObj(b,sprites.bullet));

  ctx.fillStyle="#f0f";
  enemyBullets.forEach(b=>b.is_active && drawObj(b,sprites.ebullet));

  drawHPBar(10,(SCREEN_HEIGHT+0.6)*cellH,100,player.hp,player.max_hp);
  ctx.fillStyle="#fff";
  ctx.fillText(`HP:${player.hp}/${player.max_hp}`,120,(SCREEN_HEIGHT+0.5)*cellH);
  ctx.fillText(`Score:${score}`,250,(SCREEN_HEIGHT+0.5)*cellH);
}

/* ================= MAIN LOOP ================= */
function gameLoop(){
  handleInput();
  updateBullets();
  updateEnemies();
  updateBoss();
  enemyFire();
  updateEnemyBullets();
  checkBossSpawn();
  render();
}

initGame();
setInterval(gameLoop,1000/FPS);
