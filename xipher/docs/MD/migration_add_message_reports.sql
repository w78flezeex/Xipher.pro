-- Message reports for moderation queue
CREATE TABLE IF NOT EXISTS message_reports (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    reporter_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    reported_user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    message_id UUID NOT NULL,
    message_type VARCHAR(20) DEFAULT 'direct',
    message_content TEXT NOT NULL,
    reason VARCHAR(50) NOT NULL CHECK (reason IN ('spam','abuse','other')),
    comment TEXT,
    context_json JSONB DEFAULT '[]'::jsonb,
    status VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending','resolved','dismissed')),
    resolution_note TEXT,
    resolved_by UUID REFERENCES users(id),
    resolved_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_message_reports_reporter ON message_reports(reporter_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_message_reports_status ON message_reports(status, created_at DESC);
