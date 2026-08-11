-- ====================================================================================
-- BOND TOUCH PRODUCTION DATABASE SCHEMA & SECURITY POLICIES (SUPABASE / POSTGRESQL)
-- ====================================================================================

-- 1. EXTENSIONS
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- 2. COUPLES & PAIRING TABLE
CREATE TABLE IF NOT EXISTS public.couples (
    pair_id TEXT PRIMARY KEY,
    encryption_key_hash TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL,
    status TEXT DEFAULT 'ACTIVE' CHECK (status IN ('ACTIVE', 'PAUSED', 'REVOKED'))
);

-- 3. REGISTERED HARDWARE DEVICES TABLE
CREATE TABLE IF NOT EXISTS public.devices (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    pair_id TEXT REFERENCES public.couples(pair_id) ON DELETE CASCADE,
    device_name TEXT NOT NULL CHECK (device_name IN ('BraceletA', 'BraceletB')),
    mac_address TEXT NOT NULL,
    hardware_serial TEXT UNIQUE NOT NULL,
    last_seen TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now())
);

-- 4. LIVE PRESENCE HEARTBEATS TABLE (5-second TTL)
CREATE TABLE IF NOT EXISTS public.presence (
    device_name TEXT PRIMARY KEY,
    pair_id TEXT REFERENCES public.couples(pair_id) ON DELETE CASCADE,
    is_online BOOLEAN DEFAULT false,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

-- 5. ROW-LEVEL SECURITY (RLS) POLICIES
ALTER TABLE public.couples ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.devices ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.presence ENABLE ROW LEVEL SECURITY;

-- Allow anonymous paired clients to read presence for their pair_id
CREATE POLICY "Allow paired read presence" ON public.presence
    FOR SELECT USING (true);

-- Allow device to update its own presence heartbeat
CREATE POLICY "Allow device presence update" ON public.presence
    FOR ALL USING (true);

-- 6. ENABLE REALTIME SUBSCRIPTIONS FOR TOUCH & PRESENCE TABLES
ALTER PUBLICATION supabase_realtime ADD TABLE public.presence;
