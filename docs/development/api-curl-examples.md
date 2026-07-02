# API curl examples

These examples assume the server is listening at `http://127.0.0.1:8080`.

```bash
BASE=http://127.0.0.1:8080

curl -sS "$BASE/api/health"

curl -sS -D root.headers \
  -H "Content-Type: application/json" \
  -d '{"username":"root","password":"change-me-now"}' \
  "$BASE/api/auth/login"
ROOT_TOKEN="$(awk -F': ' 'tolower($1)=="x-session-token"{gsub("\r","",$2); print $2}' root.headers)"

curl -sS -H "X-Session-Token: $ROOT_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"title":"A Plus B","statement":"Read two integers.","input_description":"a b","output_description":"a+b","samples_json":"[]","difficulty":"easy","tags":"math,intro"}' \
  "$BASE/api/admin/problems"

curl -sS -H "X-Session-Token: $ROOT_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"input":"1 2\n","output":"3\n"}' \
  "$BASE/api/admin/problems/1/testcases"

curl -sS "$BASE/api/problems?keyword=plus&difficulty=easy&tag=math"

curl -sS -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"password123"}' \
  "$BASE/api/auth/register"

curl -sS -D alice.headers \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"password123"}' \
  "$BASE/api/auth/login"
ALICE_TOKEN="$(awk -F': ' 'tolower($1)=="x-session-token"{gsub("\r","",$2); print $2}' alice.headers)"

curl -sS -H "X-Session-Token: $ALICE_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"source":"#include <iostream>\nint main(){long long a,b; if(std::cin>>a>>b) std::cout<<a+b<<\"\\n\"; return 0;}"}' \
  "$BASE/api/problems/1/submissions"

curl -sS -H "X-Session-Token: $ALICE_TOKEN" \
  "$BASE/api/submissions/1"

curl -sS -H "X-Session-Token: $ROOT_TOKEN" \
  "$BASE/api/admin/submissions"
```
