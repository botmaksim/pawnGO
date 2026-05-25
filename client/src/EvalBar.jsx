import React from 'react';

const EvalBar = ({ score, boardOrientation }) => {
  // score is a string like "0.50", "-1.20", "M3", "-M2"
  
  let fillPercentage = 50;
  let displayScore = score || "0.00";
  
  if (score) {
    if (score.startsWith('M')) {
       // Mate for white
       fillPercentage = 100;
       displayScore = score;
    } else if (score.startsWith('-M')) {
       // Mate for black
       fillPercentage = 0;
       displayScore = score;
    } else {
       const cp = parseFloat(score);
       // Cap at +/- 5 pawns for visual scale
       let cappedCp = Math.max(-5, Math.min(5, cp));
       // Map -5 -> 0%, 0 -> 50%, +5 -> 100%
       fillPercentage = 50 + (cappedCp * 10);
    }
  }

  // If black is at the bottom, we might want to invert the visual
  const isBlackBottom = boardOrientation === 'black';
  const finalFill = isBlackBottom ? 100 - fillPercentage : fillPercentage;
  
  // The text should be at the top if White is winning (and white is top), etc.
  const textIsWhite = parseFloat(score) >= 0 || score.startsWith('M');
  
  return (
    <div style={{
      width: '30px',
      height: '100%',
      backgroundColor: isBlackBottom ? '#fff' : '#333', // Empty background
      borderRadius: '4px',
      overflow: 'hidden',
      position: 'relative',
      display: 'flex',
      flexDirection: 'column',
      justifyContent: 'flex-end',
      boxShadow: 'inset 0 0 5px rgba(0,0,0,0.5)',
      marginRight: '10px'
    }}>
      <div style={{
        height: `${finalFill}%`,
        backgroundColor: isBlackBottom ? '#333' : '#fff', // Fill color
        width: '100%',
        transition: 'height 0.5s ease-out'
      }}></div>
      
      <div style={{
        position: 'absolute',
        top: textIsWhite && !isBlackBottom ? '5px' : (textIsWhite && isBlackBottom ? 'auto' : '5px'),
        bottom: textIsWhite && !isBlackBottom ? 'auto' : (textIsWhite && isBlackBottom ? '5px' : '5px'),
        left: 0,
        right: 0,
        textAlign: 'center',
        color: (textIsWhite && !isBlackBottom) || (!textIsWhite && isBlackBottom) ? '#333' : '#fff',
        fontSize: '10px',
        fontWeight: 'bold',
        zIndex: 10
      }}>
        {displayScore}
      </div>
    </div>
  );
};

export default EvalBar;
