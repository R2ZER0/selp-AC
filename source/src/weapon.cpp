// weapon.cpp: all shooting and effects code

#include "pch.h"
#include "cube.h"
#include "bot/bot.h"
#include "hudgun.h"

VARP(autoreload, 0, 1, 1);

vec sg[SGRAYS];

void updatelastaction(playerent *d)
{
    loopi(NUMGUNS) d->weapons[i]->updatetimers();
    d->lastaction = lastmillis;
}

void checkweaponswitch()
{
	if(!player1->weaponchanging) return;
    int timeprogress = lastmillis-player1->weaponchanging;
    if(timeprogress>weapon::weaponchangetime)
	{
        addmsg(SV_WEAPCHANGE, "ri", player1->weaponsel->type);
		player1->weaponchanging = 0;
	}
    else if(timeprogress>weapon::weaponchangetime/2) player1->weaponsel = player1->nextweaponsel;
}

void weaponswitch(weapon *w)
{
    if(!w) return;
    player1->weaponsel->ondeselecting();
    player1->weaponchanging = lastmillis;
    player1->nextweaponsel = w;
    w->onselecting();
}

void selectweapon(weapon *w) { if(w && w->selectable()) weaponswitch(w); }
void selectweaponi(int w) { if(w >= 0 && w < NUMGUNS) selectweapon(player1->weapons[w]); }

void shiftweapon(int s)
{
    if(player1->state == CS_ALIVE)
    {
        for(int i = 0; i < NUMGUNS && !player1->weaponchanging; i++) 
        {
            int trygun = player1->weaponsel->type + s + (s < 0 ? -i : i);
            if((trygun %= NUMGUNS) < 0) trygun += NUMGUNS;
            selectweapon(player1->weapons[trygun]);
        }
    }
    else toggledeathcam();
}

int currentprimary() { return player1->primweap->type; }
int curweapon() { return player1->weaponsel->type; }

int magcontent(int w) { if(w > 0 && w < NUMGUNS) return player1->weapons[w]->mag; else return -1;}
int magreserve(int w) { if(w > 0 && w < NUMGUNS) return player1->weapons[w]->ammo; else return -1;}

COMMANDN(weapon, selectweaponi, ARG_1INT);
COMMAND(shiftweapon, ARG_1INT);
COMMAND(currentprimary, ARG_1EST);
COMMAND(curweapon, ARG_1EXP);
COMMAND(magcontent, ARG_1EXP);
COMMAND(magreserve, ARG_1EXP);

VAR(scopefov, 5, 50, 50);
bool scoped = false;
int oldfov = 100;

void setscope(bool activate)
{
    if(player1->weaponsel->type != GUN_SNIPER || player1->state == CS_DEAD || player1->weaponsel->reloading) return;
	if(activate == scoped) return;
	if(activate)
	{
		oldfov = getvar("fov");
		setvar("fov", scopefov);
	}
	else
	{
		setvar("fov", oldfov);
	}
	scoped = activate;
}

COMMAND(setscope, ARG_1INT);

void tryreload(playerent *p)
{
    if(!p || p->state!=CS_ALIVE || p->weaponsel->reloading || p->weaponchanging) return;
    p->weaponsel->reload();
}

void selfreload() { tryreload(player1); }
COMMANDN(reload, selfreload, ARG_NONE);

void createrays(vec &from, vec &to)             // create random spread of rays for the shotgun
{
    float f = to.dist(from)*SGSPREAD/1000;
    loopi(SGRAYS)
    {
        #define RNDD (rnd(101)-50)*f
        vec r(RNDD, RNDD, RNDD);
        sg[i] = to;
        sg[i].add(r);
        #undef RNDD
    }
}

static inline bool intersect(const vec &o, const vec &rad, const vec &from, const vec &to, vec *end) // if lineseg hits entity bounding box
{
    const vec *p;
    vec v = to, w = o; 
    v.sub(from);
    w.sub(from);
    float c1 = w.dot(v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = v.squaredlen();
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            v.mul(f).add(from);
            p = &v;
        }
    }

    if(p->x <= o.x+rad.x
       && p->x >= o.x-rad.x
       && p->y <= o.y+rad.y
       && p->y >= o.y-rad.y
       && p->z <= o.z+rad.z
       && p->z >= o.z-rad.z)
    {
        if(end) *end = *p;
        return true;
    }
    return false;
}

bool intersect(dynent *d, const vec &from, const vec &to, vec *end)
{
    vec o(d->o);
    o.z += (d->aboveeye - d->dyneyeheight())/2;
    return intersect(o, vec(d->radius, d->radius, (d->aboveeye + d->dyneyeheight())/2), from, to, end);
}

bool intersect(entity *e, const vec &from, const vec &to, vec *end)
{
    mapmodelinfo &mmi = getmminfo(e->attr2);
    if(!&mmi || !mmi.h) return false;

    float lo = float(S(e->x, e->y)->floor+mmi.zoff+e->attr3);
    return intersect(vec(e->x, e->y, lo+mmi.h/2.0f), vec(mmi.rad, mmi.rad, mmi.h/2.0f), from, to, end);
}

playerent *intersectclosest(vec &from, vec &to, playerent *at)
{
    playerent *best = NULL;
    float bestdist = 1e16f;
    if(at!=player1 && player1->state==CS_ALIVE && intersect(player1, from, to))
    {
        best = player1;
        bestdist = at->o.dist(player1->o);
    }
    loopv(players)
    {
        playerent *o = players[i];
        if(!o || o==at || o->state!=CS_ALIVE) continue;
        if(!intersect(o, from, to)) continue;
        float dist = at->o.dist(o->o);
        if(dist<bestdist)
        {
            best = o;
            bestdist = dist;
        }
    }
    return best;
}

playerent *playerincrosshair()
{
    return intersectclosest(player1->o, worldpos, player1);
}

struct projectile { vec o, to; float speed; playerent *owner; int gun; bool local; int id; };
vector<projectile > projs;

void projreset() { projs.setsize(0); }

void newprojectile(vec &from, vec &to, float speed, bool local, playerent *owner, int gun, int id = lastmillis)
{
    projectile &p = projs.add();
    p.o = from;
    p.to = to;
    p.speed = speed;
    p.local = local;
    p.owner = owner;
    p.gun = gun;
    p.id = id;
}

void removeprojectiles(playerent *owner)
{
    loopv(projs) if(projs[i].owner==owner) projs.remove(i--);
}
    
void damageeffect(int damage, playerent *d)
{
    particle_splash(3, damage/10, 1000, d->o);
}

struct hitmsg
{
    int target, lifesequence, info;
    ivec dir;
};

vector<hitmsg> hits;



void hit(int damage, playerent *d, playerent *at, const vec &vel, int gun, bool gib, int info)
{
    if(d==player1 || d->type==ENT_BOT || !m_mp(gamemode)) d->hitpush(damage, vel, at, gun);

    if(!m_mp(gamemode)) dodamage(damage, d, at, gib);
    else
    {
        hitmsg &h = hits.add();
        h.target = d->clientnum;
        h.lifesequence = d->lifesequence;
        h.info = info;
        if(d==player1)
        {
            h.dir = ivec(0, 0, 0);
            d->damageroll(damage);
            damageblend(damage);
            playsound(S_PAIN6);
        }
        else 
        {
            h.dir = ivec(int(vel.x*DNF), int(vel.y*DNF), int(vel.z*DNF));
            damageeffect(damage, d);
            playsound(S_PAIN1+rnd(5), d);
        }
    }
}

void hitpush(int damage, playerent *d, playerent *at, vec &from, vec &to, int gun, bool gib, int info)
{
    vec v(to);
    v.sub(from);
    v.normalize();
    hit(damage, d, at, v, gun, gib, info);
}

float expdist(playerent *o, vec &dir, const vec &v)
{
    vec middle = o->o;
    middle.z += (o->aboveeye-o->dyneyeheight())/2;
    float dist = middle.dist(v, dir);
    dir.div(dist);
    if(dist<0) dist = 0;
    return dist;
}
 
void radialeffect(playerent *o, vec &v, int qdam, playerent *at, int gun)
{
    if(o->state!=CS_ALIVE) return;
    vec dir;
    float dist = expdist(o, dir, v);
    if(dist<EXPDAMRAD) 
    {
        int damage = (int)(qdam*(1-dist/EXPDAMRAD));
        hit(damage, o, at, dir, gun, true, int(dist*DMF)); 
    }
}

void splash(projectile &p, vec &v, vec &vold, playerent *notthis, int qdam)
{
    particle_splash(0, 50, 300, v);
    if(p.gun!=GUN_GRENADE)
    {
        playsound(S_FEXPLODE, NULL, NULL, &v);
        // no push?
    }
    else
    {
        //playsound(S_RLHIT, &v);
        particle_fireball(5, v); 
        addscorchmark(v);
        dodynlight(vold, v, 0, 0, p.owner);
        if(!p.local) return;
        radialeffect(player1, v, qdam, p.owner, p.gun);
        loopv(players)
        {
            playerent *o = players[i];
            if(!o || o==notthis) continue; 
            radialeffect(o, v, qdam, p.owner, p.gun);
        }
    }
}

bool projdamage(playerent *o, projectile &p, vec &v, int qdam)
{
    if(o->state!=CS_ALIVE || !intersect(o, p.o, v)) return false;
    splash(p, v, p.o, o, qdam);
    vec dir;
    expdist(o, dir, v);
    hit(qdam, o, p.owner, dir, p.gun, true, 0);
    return true;
}

void moveprojectiles(float time)
{
    loopv(projs)
    {
        projectile &p = projs[i];
        int qdam = guns[p.gun].damage;
        vec v;
        float dist = p.to.dist(p.o, v);
        float dtime = dist*1000/p.speed;
        if(time>dtime) dtime = time;
        v.mul(time/dtime).add(p.o);
        bool exploded = false;
        hits.setsizenodelete(0);
        if(p.local)
        {
            loopvj(players)
            {
                playerent *o = players[j];
                if(!o || p.owner==o || o->o.reject(v, 10.0f)) continue; 
                if(projdamage(o, p, v, qdam)) exploded = true;
            }
            if(p.owner!=player1 && projdamage(player1, p, v, qdam)) exploded = true;
        }
        if(!exploded)
        {
            if(time==dtime) 
            {
                splash(p, v, p.o, NULL, qdam);
                exploded = true;
            }
            else
            {
                if(p.gun==GUN_GRENADE) { dodynlight(p.o, v, 0, 255, p.owner); particle_splash(5, 2, 200, v); }
                else { particle_splash(1, 1, 200, v); particle_splash(guns[p.gun].part, 1, 1, v); }
                // Added by Rick
                traceresult_s tr;
                TraceLine(p.o, v, p.owner, true, &tr);
                if(tr.collided) splash(p, v, p.o, NULL, qdam);
                // End add                
            }       
        }
        if(exploded)
        {
            if(p.local)
                addmsg(SV_EXPLODE, "ri3iv", lastmillis, p.gun, p.id,
                    hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
            projs.remove(i--);
        }
        else p.o = v;
    }
}

vector<bounceent *> bounceents;

void removebounceents(playerent *owner)
{
    loopv(bounceents) if(bounceents[i]->owner==owner) { delete bounceents[i]; bounceents.remove(i--); }
}

void movebounceents()
{
    loopv(bounceents) if(bounceents[i])
    {
        bounceent *p = bounceents[i];
        if(p->bouncestate == NADE_THROWED || p->bouncestate == GIB) moveplayer(p, 2, false);
        if(!p->isalive(lastmillis))
        {
            p->destroy();
            delete p;
            bounceents.remove(i--);
        }
    }
}

void clearbounceents()
{   
    if(gamespeed==100);
    else if(multiplayer(false)) bounceents.add((bounceent *)player1);
    loopv(bounceents) if(bounceents[i]) { delete bounceents[i]; bounceents.remove(i--); }
}

void renderbounceents()
{
    loopv(bounceents)
    {
        bounceent *p = bounceents[i];
        if(!p) continue;
        string model;
        vec o(p->o);

        switch(p->bouncestate)
        {
            case NADE_THROWED:
                s_strcpy(model, "weapons/grenade/static");
                break;
            case GIB:
            default:
            {    
                uint n = (((4*(uint)(size_t)p)+(uint)p->timetolife)%3)+1;
                s_sprintf(model)("misc/gib0%u", n);
                int t = lastmillis-p->millis;
                if(t>p->timetolife-2000)
                {
                    t -= p->timetolife-2000;
                    o.z -= t*t/4000000000.0f*t;
                }
                break;
            }
        }
        path(model);
        rendermodel(model, ANIM_MAPMODEL|ANIM_LOOP, 0, 1.1f, o, p->yaw+90, p->pitch);
    }
}

VARP(gibnum, 0, 6, 1000); 
VARP(gibttl, 0, 5000, 15000);
VARP(gibspeed, 1, 30, 100);

void addgib(playerent *d)
{   
    if(!d) return;
    playsound(S_GIB, d);

    loopi(gibnum)
    {
        bounceent *p = bounceents.add(new bounceent());
        p->owner = d;
        p->millis = lastmillis;
        p->timetolife = gibttl+rnd(10)*100;
        p->bouncestate = GIB;

        p->o = d->o;
        p->o.z -= d->aboveeye;
    
        p->yaw = (float)rnd(360);
        p->pitch = (float)rnd(360);

        p->maxspeed = 30.0f;
        p->rotspeed = 3.0f;

        const float angle = (float)rnd(360);
        const float speed = (float)gibspeed;

        p->vel.x = sinf(RAD*angle)*rnd(1000)/1000.0f;
        p->vel.y = cosf(RAD*angle)*rnd(1000)/1000.0f;
        p->vel.z = rnd(1000)/1000.0f;
        p->vel.mul(speed/100.0f);
    }
}

void shorten(vec &from, vec &to, vec &target)
{
    target.sub(from).normalize().mul(from.dist(to)).add(from);
}

const float HEADSIZE = 0.8f;
vec hitpos;

void raydamage(vec &from, vec &to, playerent *d)
{
    int gdam = d->weaponsel->info.damage;
    playerent *o = NULL;
    if(d->weaponsel->type==GUN_SHOTGUN)
    {
        uint done = 0;
        playerent *cl = NULL;
        for(;;)
        {
            bool raysleft = false;
            int hitrays = 0;
            o = NULL;
            loop(r, SGRAYS) if((done&(1<<r))==0 && (cl = intersectclosest(from, sg[r], d)))
            {
                if(!o || o==cl)
                {
                    hitrays++;
                    o = cl;
                    done |= 1<<r;
                    shorten(from, o->o, sg[r]);
                }
                else raysleft = true;
            }
            if(hitrays) hitpush(hitrays*gdam, o, d, from, to, d->weaponsel->type, false, hitrays);
            if(!raysleft) break;
        }
    }
    else if((o = intersectclosest(from, to, d)))
    {
        bool gib = false;
        if(d->weaponsel->type==GUN_KNIFE) gib = true;
    	else if(d==player1 && d->weaponsel->type==GUN_SNIPER
        && worldpos!=hitpos && hitpos.z>=o->o.z+o->aboveeye-HEADSIZE
        && intersect(o, from, hitpos)) 
        {
            gdam *= 3;
            gib = true;
        }

        hitpush(gdam, o, d, from, to, d->weaponsel->type, gib, gib ? 1 : 0);
        shorten(from, o->o, to);
    }
}

weapon::weapon(struct playerent *owner, int type) : type(type), info(guns[type]), owner(owner), reloading(0),
    ammo(owner->ammo[type]), mag(owner->mag[type]), gunwait(owner->gunwait[type])
{
}

const int weapon::weaponchangetime = 400;
const float weapon::weaponbeloweye = 0.2f;

void weapon::sendshoot(vec &from, vec &to)
{
    if(owner!=player1) return;
    addmsg(SV_SHOOT, "ri2i6iv", lastmillis, owner->weaponsel->type,
           (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), 
           (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF), 
           hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
}

bool weapon::modelattacking()
{
    // FIXME!!!
    int animtime = info.attackdelay; //min(owner->gunwait[owner->weaponsel->type], owner->weaponsel->info.attackdelay);
    if(lastmillis - owner->lastaction < animtime) return true;
    else return false;
}

void weapon::attacksound()
{
    if(info.sound != S_NULL) playsound(info.sound, owner==player1 ? NULL : owner);
}

bool weapon::reload()
{
    if(mag>=info.magsize || ammo<=0) return false;
    updatelastaction(owner);
    reloading = lastmillis;
    gunwait += info.reloadtime;
    
    int numbullets = min(info.magsize - mag, ammo);
	mag += numbullets;
	ammo -= numbullets;

	if(owner->type==ENT_BOT) playsound(info.reload, owner);
	else playsoundc(info.reload);
    if(owner==player1) addmsg(SV_RELOAD, "ri2", lastmillis, owner->weaponsel->type);
    return true;
}

void weapon::renderstats()
{
    char gunstats[64];
    sprintf(gunstats, "%i/%i", mag, ammo);
    draw_text(gunstats, 690, 827);    
}

void weapon::attackphysics(vec &from, vec &to) // physical fx to the owner
{
    const guninfo &g = info;
    vec unitv;
    float dist = to.dist(from, unitv);
    float f = dist/1000;
    int spread = dynspread();
    float recoil = dynrecoil()*-0.01f;

    // spread
    if(spread>1)
    {
        #define RNDD (rnd(spread)-spread/2)*f
        vec r(RNDD, RNDD, RNDD);
        to.add(r);
        #undef RNDD
    }
    // kickback & recoil
    owner->vel.add(vec(unitv).mul(recoil/dist).mul(owner->crouching ? 0.75 : 1.0f));
    owner->pitchvel = min(pow(shots/(float)(g.recoilincrease), 2.0f)+(float)(g.recoilbase)/10.0f, (float)(g.maxrecoil)/10.0f);
}

void weapon::renderhudmodel(int lastaction, int index) 
{ 
    vec unitv;
    float dist = worldpos.dist(owner->o, unitv);
    unitv.div(dist);
    
	weaponmove wm;
	if(!intermission) wm.calcmove(unitv, lastaction);
    s_sprintfd(path)("weapons/%s", info.modelname);
    static int lastanim[2], lastswitch[2];
    if(lastanim[index]!=(wm.anim|(type<<16)))
    {
        lastanim[index] = wm.anim|(type<<16);
        lastswitch[index] = lastmillis;
    }
    rendermodel(path, wm.anim|(index ? ANIM_MIRROR : 0), 0, 0, wm.pos, player1->yaw+90, player1->pitch+wm.k_rot, 40.0f, lastswitch[index], NULL, NULL, 1.28f);  
}

void weapon::updatetimers()
{
    if(gunwait) gunwait = max(gunwait - (lastmillis-owner->lastaction), 0);
}

void weapon::onselecting() 
{ 
    owner->lastaction = lastmillis;
    playsound(S_GUNCHANGE); 
}

void weapon::renderhudmodel() { renderhudmodel(owner->lastaction); }
int weapon::dynspread() { return info.spread; }
int weapon::dynrecoil() { return info.recoil; }
bool weapon::selectable() { return !reloading && this != owner->weaponsel && owner->state == CS_ALIVE && !owner->weaponchanging; }



struct grenadeent : bounceent
{
    grenadeent (playerent *owner, int millis = 0)
    {
        ASSERT(owner);
        bounceent::owner = owner;
        bounceent::millis = lastmillis;
        timetolife = 2000-millis;
        bouncestate = NADE_ACTIVATED;
	    maxspeed = 27.0f;
        rotspeed = 6.0f;
    }

    void explode()
    {
        if(bouncestate != NADE_THROWED) owner->weapons[GUN_GRENADE]->attack(vec(0,0,0));
        playsound(S_FEXPLODE, NULL, NULL, &o);
        newprojectile(o, o, 1, owner==player1, owner, GUN_GRENADE, millis);
    }

    virtual void destroy()
    {
        if(bouncestate==NADE_ACTIVATED || bouncestate==NADE_THROWED) explode();
    }
};

struct grenades : weapon
{
    const int throwwait;
    int throwmillis;

    grenades(playerent *owner) : weapon(owner, GUN_GRENADE), throwwait((13*1000)/40), throwmillis(0) {}

    bool attack(vec &targ)
    {
        int attackmillis = lastmillis-owner->lastaction;
        vec &from = owner->o;
        vec &to = targ;

		if(throwmillis && attackmillis >= throwwait)
		{
            throwmillis = 0;
            if(!mag)
            {
                owner->weaponchanging = lastmillis-1-(weaponchangetime/2);
                owner->nextweaponsel = owner->weaponsel = owner->primweap;
            }
            return false;
		}

		if(attackmillis<gunwait) return false;

        gunwait = reloading = 0;

		if(owner->attacking && !owner->inhandnade) // activate
		{
			if(!mag) return false;
            throwmillis = 0;
            owner->inhandnade = bounceents.add(new grenadeent(owner)); 
            updatelastaction(owner);
            mag--;
            gunwait = info.attackdelay;
            owner->lastattackweapon = this;
            addmsg(SV_SHOOT, "ri2i6i", lastmillis, owner->weaponsel->type,
                   (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), 
                   (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
                   0);
            playsound(S_GRENADEPULL);
		}
        else if(!owner->attacking && owner->inhandnade && attackmillis>info.attackdelay) thrownade();
        return true;
    }

    void attackfx(vec &from, vec &to, int millis) // other player's grenades
    {
        throwmillis = millis;
        if(millis == 0) playsound(S_GRENADEPULL, owner); // activate
        else if(millis > 0) // throw
        {
            owner->inhandnade = bounceents.add(new grenadeent(owner, millis)); 
            thrownade(from, to, owner->inhandnade);
        }
    }

    void selecting() { throwmillis = 0; }
    void onammopicked() { throwmillis = 0; }

    int modelanim()
    {
        if(throwing()) return ANIM_GUN_THROW;
        else
        {
            int animtime = min(gunwait, info.attackdelay);
            if(inhand() || lastmillis - owner->lastaction < animtime) return ANIM_GUN_SHOOT;
        }
        return ANIM_GUN_IDLE;
    }

    bool throwing() { return throwmillis && !owner->inhandnade; }
    bool inhand() { return throwmillis <= 0 && owner->inhandnade; }

    void thrownade()
    {
	    const float speed = cosf(RAD*owner->pitch);
	    vec vel(sinf(RAD*owner->yaw)*speed, -cosf(RAD*owner->yaw)*speed, sinf(RAD*owner->pitch));
        vel.mul(1.5f);
        vec from(vel);
        from.mul(1.1f*owner->radius);
        from.add(owner->o);
        thrownade(from, vel, owner->inhandnade);
    }

    void thrownade(const vec &from, const vec &vel, bounceent *p)
    {
        if(!p) return;
        p->vel = vel;
        p->o = from;
        p->bouncestate = NADE_THROWED;
        loopi(10) moveplayer(p, 10, true, 10);

	    p->bouncestate = NADE_THROWED;
        owner->inhandnade = NULL;

        throwmillis = lastmillis;

        if(owner==player1)
        {
            playsound(S_GRENADETHROW);
            updatelastaction(player1);
            addmsg(SV_THROWNADE, "ri7", int(p->o.x*DMF), int(p->o.y*DMF), int(p->o.z*DMF), int(p->vel.x*DMF), int(p->vel.y*DMF), int(p->vel.z*DMF), lastmillis-p->millis);
        }
        else playsound(S_GRENADETHROW, owner);
    }

    void drawstats()
    {
        char gunstats[64];
        sprintf(gunstats, "%i", mag);
        draw_text(gunstats, 690, 827);  
    }

    bool selectable() { return weapon::selectable() && !inhand() && mag; }
};

struct gun : weapon
{
    gun(playerent *owner, int type) : weapon(owner, type) {}

    virtual bool attack(vec &targ)
    {
	    int attackmillis = lastmillis-owner->lastaction;
		if(attackmillis<gunwait) return false;
        gunwait = reloading = 0;
		
        if(!owner->attacking) { shots = 0;  return false; }
        updatelastaction(owner);
        if(!mag) 
        { 
            playsoundc(S_NOAMMO);
            ASSERT(owner->type!=ENT_BOT);
		    gunwait += 250;
		    owner->lastattackweapon = NULL;
            shots = 0; 
            checkautoreload(); 
            return false; 
        }

        owner->lastattackweapon = this;
		shots++;

		if(!info.isauto) owner->attacking = false;
	    
        vec from = owner->o;
	    vec to = targ;
        from.z -= weaponbeloweye;

	    attackphysics(from, to);

        hits.setsizenodelete(0);
        raydamage(from, to, owner);
	    attackfx(from, to, 0);

        gunwait = info.attackdelay;
        mag--;

        sendshoot(from, to); 
        return true;
    }

    virtual void attackfx(vec &from, vec &to, int millis) 
    {
        addbullethole(from, to);
        addshotline(owner, from, to);
        particle_splash(0, 5, 250, to);
        attacksound();
    }

    int modelanim() { return modelattacking() ? ANIM_GUN_SHOOT|ANIM_LOOP : ANIM_GUN_IDLE; };

    void checkautoreload()
    {
        if(autoreload && owner==player1 && !mag) reload();
    }
};

struct shotgun : gun
{
    shotgun(playerent *owner) : gun(owner, GUN_SHOTGUN) {}   

    bool attack(vec &targ)
    {
        vec from = owner->o;
		from.z -= weaponbeloweye;
        createrays(from, targ);
        return gun::attack(targ);
    }

    void attackfx(vec &from, vec &to, int millis)
    {
        loopi(SGRAYS) particle_splash(0, 5, 200, sg[i]);
        if(addbullethole(from, to))
        {
            int holes = 3+rnd(5);
            loopi(holes) addbullethole(from, sg[i], 0);
        }
        attacksound();
    }

    bool selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }
};

struct subgun : gun
{
    subgun(playerent *owner) : gun(owner, GUN_SUBGUN) {}   
    bool selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }
};

struct sniperrifle : gun
{
    sniperrifle(playerent *owner) : gun(owner, GUN_SNIPER) {}   

    void attackfx(vec &from, vec &to, int millis)
    {
        addbullethole(from, to);
        addshotline(owner, from, to);
        particle_splash(0, 50, 200, to);
        particle_trail(1, 500, from, to);
        attacksound();
    }

    bool reload()
    {
        bool r = weapon::reload();
        if(owner==player1 && r) setscope(false);
        return r;
    }

    int dynspread() { return scoped ? 1 : info.spread; }
    int dynrecoil() { return scoped ? info.recoil / 3 : info.recoil; }
    bool selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }
    void onselecting() { weapon::onselecting(); setscope(false); }
    void ondeselecting() { setscope(false); }
};

struct assaultrifle : gun
{
    assaultrifle(playerent *owner) : gun(owner, GUN_ASSAULT) {}

    int dynspread() { return shots > 3 ? 70 : info.spread; }
    int dynrecoil() { return info.recoil + (rnd(8)*-0.01f); }
    bool selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }
};

struct pistol : gun
{
    pistol(playerent *owner) : gun(owner, GUN_PISTOL) {}
    bool selectable() { return weapon::selectable() && !m_nopistol; }
};

struct akimbo : gun
{
    akimbo(playerent *owner) : gun(owner, GUN_AKIMBO), akimbomillis(0)
    {
        akimbolastaction[0] = akimbolastaction[1] = 0;
    }
        
    bool akimboside;
    int akimbomillis;
    int akimbolastaction[2];

    bool attack(vec &targ)
    {
        if(gun::attack(targ))
        {
			akimbolastaction[akimboside?1:0] = lastmillis;
			akimboside = !akimboside;
            return true;
        }
        return false;
    }

    void onammopicked()
    {
        akimbomillis = lastmillis + 30000;
        if(owner==player1)
        {
            if(owner->weaponsel->type!=GUN_SNIPER && !owner->inhandnade) weaponswitch(this);
            addmsg(SV_AKIMBO, "ri", lastmillis);
        }
    }

    void onselecting() 
    { 
        gun::onselecting();
        akimbolastaction[0] = akimbolastaction[1] = lastmillis; 
    }

    bool selectable() { return weapon::selectable() && !m_nopistol && owner->akimbo; }
    void updatetimers() { weapon::updatetimers(); /*loopi(2) akimbolastaction[i] = lastmillis;*/ }
    void reset() { akimbolastaction[0] = akimbolastaction[1] = akimbomillis = 0; akimboside = false; }

    void renderhudmodel()
    {       
        weapon::renderhudmodel(akimbolastaction[0], 0);
        weapon::renderhudmodel(akimbolastaction[1], 1);
    }

    bool timerout() { return akimbomillis && akimbomillis <= lastmillis; }
};

struct knife : weapon
{
    knife(playerent *owner) : weapon(owner, GUN_KNIFE) {}

    bool attack(vec &targ)
    {
	    int attackmillis = lastmillis-owner->lastaction;
		if(attackmillis<gunwait) return false;
        gunwait = reloading = 0;
		
        if(!owner->attacking) return false;
        updatelastaction(owner);

        owner->lastattackweapon = this;
		owner->attacking = false;
	    
        vec from = owner->o;
	    vec to = targ;
        from.z -= weaponbeloweye;

        vec unitv;
        float dist = to.dist(from, unitv);
        unitv.div(dist);
        unitv.mul(3); // punch range
	    to = from;
        to.add(unitv);
 
        hits.setsizenodelete(0);
        raydamage(from, to, owner);
        attackfx(from, to, 0);
        sendshoot(from, to); 
        gunwait = info.attackdelay;
        return true;
    };

    int modelanim() { return modelattacking() ? ANIM_GUN_SHOOT : ANIM_GUN_IDLE; };

    void drawstats() {}
    void attackfx(vec &from, vec &to, int millis) { attacksound(); }
};


void shoot(playerent *p, vec &targ)
{
    if(p->state==CS_DEAD || p->weaponchanging) return;
    weapon *weap = p->weaponsel;
    if(weap) weap->attack(targ);
}

void weapon::equipplayer(playerent *pl)
{
    if(!pl) return;
    pl->weapons[GUN_ASSAULT] = new assaultrifle(pl);
    pl->weapons[GUN_GRENADE] = new grenades(pl);
    pl->weapons[GUN_KNIFE] = new knife(pl);
    pl->weapons[GUN_PISTOL] = new pistol(pl);
    pl->weapons[GUN_SHOTGUN] = new shotgun(pl);
    pl->weapons[GUN_SNIPER] = new sniperrifle(pl);
    pl->weapons[GUN_SUBGUN] = new subgun(pl);
    pl->weapons[GUN_AKIMBO] = new akimbo(pl);
    pl->selectweapon(GUN_ASSAULT);
    pl->setprimary(GUN_ASSAULT);
    pl->setnextprimary(GUN_ASSAULT);
}

void checkakimbo()
{
    if(player1->akimbo)
    {
        akimbo &a = *((akimbo *)player1->weapons[GUN_AKIMBO]);
        if(a.timerout())
        {
            weapon &p = *player1->weapons[GUN_PISTOL]; 
            player1->akimbo = false;
            a.reset();
            // transfer ammo to pistol
            p.mag = min(p.info.magsize, max(a.mag, p.mag));
            p.ammo = max(p.ammo, p.ammo);
            if(player1->weaponsel->type==GUN_AKIMBO) weaponswitch(&p);
	        playsoundc(S_AKIMBOOUT);
        }
    }
}