# YNO-specific commands

ynoengine provides the following commands:

###### `YnoAsyncRpc`

```js
// sends (42, 69, "frobnicate") to server and continue execution
@raw 5000, 0, 42, 0, 69, "frobnicate"
// when response is received, assigns the status code to V[25]
@raw 5000, 0, 42, 0, 69,
    0, 25, "..."
// assigns the server's response to Str[27]
// and repeat this command until response is received
// parallel events execution recommended
@raw 5000, 0, 42, 0, 69, 0, 0,
    0, 7, 0, 27, "..."
// sends (42, 69, Str[29]) to server
@raw 5000, 0, 42, 0, 69, 0, 0, 0, 0, 0, 0,
    0, 29, ""
```

## Usage

Add this to your EasyRPG.ini, which will also enable Maniacs:

```ini
[Patch]
YNO = 1
```
