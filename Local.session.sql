-- SELECT *
-- FROM clio.objects
-- WHERE key >= 0x79C992D7020E038D8B6B902801078B0BA7A74C42000000000000000000000000
--     AND key < 0x79C992D7020E038D8B6B902801078B0BA7A74C42FFFFFFFFFFFFFFFFFFFFFFFF ALLOW FILTERING
SELECT key
FROM successor
WHERE token(key) >= 1000
GROUP BY key
LIMIT 10000
