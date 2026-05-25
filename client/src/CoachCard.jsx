import React from 'react';

export default function CoachCard({ coachMessage, explainData }) {
    if (!coachMessage) return null;

    let factors = [];
    if (explainData) {
        factors = [
            { name: 'Материал', key: 'material', value: explainData.material },
            { name: 'Активность', key: 'activity', value: explainData.activity },
            { name: 'Безопасность Короля', key: 'king_safety', value: explainData.king_safety },
            { name: 'Контроль Центра', key: 'center_control', value: explainData.center_control },
        ];
    }

    return (
        <div className="glass-panel coach-card" style={{ marginTop: '1rem', borderLeft: '4px solid #1baca6' }}>
            <h3 style={{ margin: '0 0 10px 0', display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span>🤖</span> Мнение Тренера (LLM AI)
            </h3>
            <p style={{ margin: 0, fontSize: '1.1rem', lineHeight: '1.4' }}>
                {coachMessage}
            </p>
            
            {factors.length > 0 && (
                <div style={{ display: 'flex', gap: '1rem', marginTop: '1rem', fontSize: '0.85rem', color: '#aaa', flexWrap: 'wrap' }}>
                    {factors.map(f => (
                        <div key={f.key} style={{ background: 'rgba(0,0,0,0.2)', padding: '4px 8px', borderRadius: '4px' }}>
                            {f.name}: <span style={{ color: f.value > 0 ? '#4CAF50' : f.value < 0 ? '#f44336' : '#aaa', fontWeight: 'bold' }}>
                                {f.value > 0 ? '+' : ''}{f.value.toFixed(2)}
                            </span>
                        </div>
                    ))}
                </div>
            )}
        </div>
    );
}
