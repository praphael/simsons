FROM node:20

WORKDIR /home/app

RUN apt-get update && \
    apt-get upgrade && \
    apt-get install curl yarnpkg

RUN yarn add sqlite3 express cookie-session
