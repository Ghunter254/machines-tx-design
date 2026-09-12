FROM gcc:13 AS build
WORKDIR /app
COPY . .
RUN mkdir -p build && gcc -std=c11 -O2 -Wall -Wextra -Iinclude src/config.c src/frame_design.c src/no_load_current_design.c src/lv_windings_design.c src/hv_windings_design.c src/performance_design.c src/tank_design.c src/simulation.c src/optimizer.c src/report.c src/main.c -lm -o build/txsim

FROM node:22-alpine
WORKDIR /app
COPY --from=build /app/build/txsim ./build/txsim
COPY backend ./backend
COPY package.json ./package.json
COPY dist ./dist
COPY data/config.txt ./data/config.txt
RUN mkdir -p data/runs
ENV TXC_PORT=5173
EXPOSE 5173
CMD ["node", "backend/server.mjs"]
