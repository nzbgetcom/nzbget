## API-method `fetchfeed`

### Signature

``` c++
bool fetchfeed(int ID);
```

### Description

Requests fetching of the RSS feed with the given ID. This is the same action as the **Fetch** button on the feeds page.

The method does not wait for the feed to be fetched and does not report whether the fetch succeeded. The items of the last fetch can be retrieved with [`viewfeed`](VIEWFEED.md); whether a fetch is currently running is reported by `FeedActive` in [Status](STATUS.md).

### Arguments

- **ID** `(int)` - ID of the feed. The ID is the number in the option name, so `Feed1.URL` belongs to the feed with ID `1`. `0` fetches all configured feeds.

### Return value

Always `true`.

### Errors

- `2` - `Invalid parameter (ID)`.

### Example

Request:

``` json
{"method": "fetchfeed", "params": [1], "id": 1}
```

Response:

``` json
{"result": true, "id": 1}
```
