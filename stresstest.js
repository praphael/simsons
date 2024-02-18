const axios=require('axios');
//const axios = require('axios'); // legacy way

const baseURL = 'http://127.0.0.1:3000/'

/*
axios.get(baseURL + 'client.html')
  .then(function (response) {
    // handle success
    console.log(response);
  })
  .catch(function (error) {
    // handle error
    console.log(error);
  })
  .finally(function () {
    // always executed
  });
*/

let userTokens = new Map();
async function loginUser(userName) {
    try {
        const response = await axios.post(baseURL + 'login/' + userName, {
            "passwd": ''
        });
        // console.log(JSON.stringify(response.data));
        if(!(response.data.hasOwnProperty("usertoken"))) {
          console.log("error 'usertoken' not present in data");
        } else {
          token = response.data.usertoken;
          // console.log(userName + " token= " + token);
          userTokens.set(userName, token);
        }
    }
    catch(error) {
        console.log(error);
    };
}


async function getFeed(userName, token) {
    try {
        const response = await axios.post(baseURL + 'getfeed/' + userName, {
            "user_token": token });
        
        feed = response.data.posts;
        // console.log(feed);
    }
    catch(error) {
        console.log(error);
    };
}

async function postMsg(userName, token, msg) {
    try {
        const response = await axios.post(baseURL + 'postmsg/' + userName, {
            "user_token": token, 
            "msg" : msg });
        feed = response.data.posts;
        // console.log(feed);
    }
    catch(error) {
        console.log(error);
    };
}

const numUsers = 1;
const numPosts = 10; 

async function loginAllUsers() {
    proms = [];
    const batchSize = Math.min(numUsers, 2);
    for(i=1; i<=numUsers; i++) {
        userName = 'user' + i
        proms.push(loginUser(userName));
        if (i % batchSize == 0) {
            await Promise.all(proms).then(() => proms = []);
        }
    }
}

async function getAllFeeds() {
    for(i=1; i<=numUsers; i++) {
        userName = 'user' + i;
        if (userTokens.has(userName)) 
          await getFeed(userName, userTokens.get(userName));
        else 
          console.log(userName + " has no token in userTokens");
    }
}

async function makePosts() {
    proms = [];
    const batchSize = Math.min(numPosts, 5);
    for(i=1; i<=numPosts; i++) {
        j = i % numUsers + 1;
        userName = 'user' + j;
        msg = userName + "post" + i + "_"
        proms.push(postMsg(userName, userTokens.get(userName), msg));
        if (i % batchSize == 0) {
            await Promise.all(proms).then(() => proms = []);
        }
    }
}


async function stressTest() {
    const t1 = performance.now();
    await loginAllUsers();
    const t2 = performance.now();
    let dt = t2-t1;
    rps = numUsers/(dt/1000);
    console.log("All users logged in dt= " + dt + " ms (" + rps + " RPS)");

    const t3 = performance.now();
    await getAllFeeds(userTokens);
    const t4 = performance.now();
    dt = t4-t3;
    rps = numUsers/(dt/1000);
    console.log("Got all users feeds dt= " + dt + " ms (" + rps + " RPS)");

    const t5 = performance.now();
    await makePosts(userTokens);
    const t6 = performance.now();
    dt = t6-t5;
    rps = numPosts/(dt/1000);
    console.log("Made all posts  dt= " + dt + " ms (" + rps + " RPS)");
}

stressTest();

/*
// Optionally the request above could also be done as
axios.get('/user', {
    params: {
      ID: 12345
    }
  })
  .then(function (response) {
    console.log(response);
  })
  .catch(function (error) {
    console.log(error);
  })
  .finally(function () {
    // always executed
  });

// Want to use async/await? Add the `async` keyword to your outer function/method.
async function getUser() {
  try {
    const response = await axios.get('/user?ID=12345');
    console.log(response);
  } catch (error) {
    console.error(error);
  }
}
*/