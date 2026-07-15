DO $$
BEGIN
    RAISE EXCEPTION
        'account identity/storefront migration is forward-only; restore a verified backup instead';
END
$$;
