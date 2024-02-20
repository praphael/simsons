const sqlite3 = require('sqlite3').verbose();

const db = new sqlite3.Database('/tmp/simsoms.db');
const numUsers = 100
const postsPerUser = 10
const subsPerUser = 5
const loremIpsum = 'Lorem ipsum dolor sit amet, consectetur adipiscing elit, \
sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. '



db.serialize(() => {
    tables = ['posts', 'subs'];
    tables.forEach((t) => {
        db.run("DROP TABLE IF EXISTS " + t, (e) => { 
            if(e) console.log("cannot drop table '" + t + "'")
        }); 
    });
    
    db.run('CREATE TABLE subs (\
            username varchar(32),\
            sub_to varchar(32)\
            )', (e) => { 
                if(e) console.log("ERROR create table 'subs'");
                else console.log("create table 'subs' success");
            }); 

    db.run('CREATE TABLE posts ( \
            author varchar(32), \
            content text, \
            post_time_utc timestamptz \
            )', (e) => {
                if(e) console.log("ERROR create table 'posts'");
                else console.log("create table 'posts' success");
            }); 

    // INSERT INTO subs (username, sub_to) VALUES ('{uname}', '{sname}'
    const stmt = db.prepare("INSERT INTO subs (username, sub_to) VALUES (?, ?)", (e) => { 
        if(e) {
            console.log("ERROR db.prepare INSERT 'subs'");
            console.log(e);
        }
    });
        
    for (let i = 1; i <= numUsers; i++) {
        for (let j = 1; j <= subsPerUser; j++) {
            sub = (i+j-1)%numUsers + 1
            const uname = "user" + i;
            const sname = "user" + sub;
            stmt.run(uname, sname, (e) => { 
                if(e) {
                    console.log("ERROR insert subs '" + uname + "' '" + sname + "'");
                    console.log(e);
                }
                else if (i%10 == 0 && j == subsPerUser) {
                    console.log("Added '" + uname + "'");
                }
            }); 
        }
        
    }
    stmt.finalize((e) => {
        console.log("stmt.finalize finished");
    });

    const t1 = Date.now();
    //const stmt2 = db.prepare("INSERT INTO posts (author, content, post_time_utc) VALUES ('{?}', '{?}', strftime('{?}', '%FT%R:%fZ'));", (e) => { 
    //strftime($t, '%FT%R:%fZ')
    const stmt2 = db.prepare("INSERT INTO posts (author, content, post_time_utc) VALUES ($a, $c, $t)", (e) => { 
        if(e) {
            console.log("ERROR db.prepare INSERT 'posts'");
            console.log(e);
        }
    });
    for (let i = 1; i <= numUsers; i++) {
        const uname = "user" + i;
        for (let j = 1; j <= postsPerUser; j++) {
            // space posts an hour apart
            t2 = t1 - j*3600*1000;
            
            sub = (i+j-1)%numUsers + 1;
            //console.log(t2.toISOString());
            postxt = uname + "_post" + j + " " + loremIpsum
            r = {$a : uname, $c : postxt, $t : t2 / 1000};
            //console.log(r);
            stmt2.run(r, (e) => { 
                if(e) {
                    console.log("ERROR insert posts '" + uname + "' post " + j);
                    console.log(e);
                }
                else if (j == postsPerUser && i%10 == 0) {
                    console.log("Added posts for '" + uname + "'");
                }
            }); 
        }
    } 
    stmt2.finalize((e) => {
        if(e)
        console.log("stmt2.finalize finished");
    });
    
});

db.close();
