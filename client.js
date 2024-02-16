
// document.onload = render();
// document.addEventListener("DOMContentLoaded", onInitialLoad);

function onInitialLoad() {
    if(!window.hasOwnProperty("appState"))
        var appState={isLoggedIn:false, username:"", usertoken:"", posts:[] };
    console.log("onInitialLoad() appState= " + JSON.stringify(appState));
    renderLogin();
}

function onReload() {
    console.log("onReload() appState.isLoggedIn= " + appState.isLoggedIn + " username="+ appState.username);
    if (appState.isLoggedIn) 
        renderFeed();
    else renderLogin();
}

function renderLogin() {
    h3 = document.createElement("h3");
    h3.setAttribute("class", "d-flex align-items-center justify-content-center");
    h3.innerHTML="Foo to Simsoms";
    div1 = document.createElement("div");
    div1.setAttribute("class", "h-100 d-flex align-items-center justify-content-center");
    frm = document.createElement("form");
    frm.setAttribute("id", "login_form");
    // frm.setAttribute("action", "../login");
    frm.setAttribute("method", "get");
    username_lbl = document.createElement("label");
    username_lbl.setAttribute("for", "username");
    username_lbl.setAttribute("class", "form-label");
    username_lbl.innerHTML = "Username:";
    username = document.createElement("input");
    username.setAttribute("type", "text");
    username.setAttribute("class", "form-control");
    username.setAttribute("id", "username");
    passwd_lbl = document.createElement("label");
    passwd_lbl.setAttribute("for", "passwd");
    passwd_lbl.setAttribute("class", "form-label");
    passwd_lbl.innerHTML = "Password:";
    passwd = document.createElement("input");
    btn = document.createElement("input");
    btn.setAttribute("type", "submit");
    btn.setAttribute("class", "btn btn-primary");
    frm.setAttribute("onsubmit", "onLoginSubmit()");
    btn.innerHTML="Login";
    login_msg = document.createElement("div");
    login_msg.setAttribute("id", "login_msg");
    login_msg.setAttribute("class", "d-flex align-items-center justify-content-center");
    passwd.setAttribute("type", "password");
    passwd.setAttribute("class", "form-control");
    passwd.setAttribute("id", "passwd");
    frm.replaceChildren(username_lbl, username, passwd_lbl, passwd, btn);
    div1.replaceChildren(frm);
    document.getElementById("main_div").replaceChildren(h3, div1, login_msg);
}

async function onLoginSubmit() {
    const uname_ctl = document.getElementById("username");
    const username = uname_ctl.value;
    const url = "../login/" + username;
    req_body = {passwd : ""};
    console.log("calling fetch()");
    post_data = { method:"POST",
        headers: {
            "Content-Type": "application/json"
        }, 
        body: JSON.stringify(req_body) };
    console.log("post_data= " + JSON.stringify(post_data));
    let data = await fetch(url, post_data)
        .then((res) => {
            statusCode = res.status; 
            return res.text();
        })
        .then(data => {
            return JSON.parse(data);
        })
        .catch(error => {
            console.error(error);
    });
    login_msg = document.getElementById("login_msg");
    if (statusCode == 200) {
        console.log("Login success data.usertoken= " + data.usertoken);
        appState.isLoggedIn = true;
        appState.username = username;
        appState.usertoken = data.usertoken;
        login_msg.innerText = "Login success!";
        await fetchFeed();
        renderFeed();
    }
    else {
        appState.isLoggedIn = false;
        login_msg.innerText = "Login failed";
    }
    // loginfrm.setAttribute("action", "../login/" + username);
}

async function fetchFeed() {
    const url = "../getfeed/" + appState.username;
    req_body = {user_token: appState.usertoken };
    post_data = { method:"POST",
        headers: {
            "Content-Type": "application/json"
        }, 
        body: JSON.stringify(req_body) };
    
    console.log("fetchFeed post_data= " + JSON.stringify(post_data));
    console.log("fetchFeed calling fetch() url=" + url);
    let data = await fetch(url, post_data)
        .then((res) => {
            statusCode = res.status; 
            return res.text();
        })
        .then(data => {
            return JSON.parse(data);
        })
        .catch(error => {
            console.error(error);
    });

    if (statusCode == 200) {
        console.log("fetchFeed fetch success");
        appState.posts = data.posts
    }
}

function renderFeed() {
    /*
    div1 = document.createElement("div");
    div1.setAttribute("id", "div1");
    div1.setAttribute("class", "d-flex align-items-center justify-content-center");
*/
    appState.posts.forEach(ele => {
        console.log(ele);
        /*
        [u, p, t] = ele;
        pdiv = document.createElement("div");
        usr = document.createElement("div");
        usr.innerText = u;
        pst = document.createElement("div");
        pst.innerText = p;
        tm = document.createElement("div");
        tm.innerText = t;
        pdiv.replaceChildren(usr, pst, tm);
        div1.appendChild(pdiv);
        */
    });

    // document.getElementById("main_div").replaceChildren(div1);
    console.log("renderFeed() success");
    console.log("renderFeed() appState.isLoggedIn= " + appState.isLoggedIn + " username="+ appState.username);    
}