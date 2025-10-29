# Hidden Bomb Card Game 

> by Arduino R4 WiFi
> Physical Computing - KMITL

## Components
- RFID/NFC Scanner: RFID-RC522
- Display: OLED i2c 1.3 inch
- Buttons: all are pull down hold button 
  - Red button (often used for: GO next, RETURN menu, PASS card) 🔴
  - left/right black button (often used for: scroll menu)
- Buzzer: passive buzzer (multiple tone)
- RGB LED: common anode RGB LED which connect PWM pins


### Bomb Roulette
> 3 or more cards recommend
1. pick 1 card to assign __Bomb__
2. Insert Bomb card to other card
3. Shuffle them for 5 seconds
4. Pick any card
  - if Normal card = SAFE ✅, put that card to used card (and then continue play)
  - if Bomb card = FAIL ❌, game over (return to menu)

### Hidden Defuse
> find Defuse card before timeout
1. pick 1 card to assign __Defuse__
2. **set Time left** (left -, GO=ready, right +)
3. mix bomb in cards
4. shuffle cards
5. Pick any card before timeout
  - if Defuse card = stop countdown and win ✅
  - if Normal card = warning not defuse ⚠️
  - timeout = Bomb is exploded, FAIL ❌

### Hold a Bomb
> don't make your friend reach 0 to you
> otherwise you were bombed
1. **set Count** (left -, GO=next, right +)
2. **set decrease _ times** (left -, GO=go, right +)
  - can make holding decrease faster
3. scan => hold & Start the game
  - **HOLDING**: who holding bomb from Count to 0
    - scan back to make bomb **IDLE**
    - if 1 (from Count 2) and scan back = decrease count for bomb ✔️
    - if 0 but not scan = that person FAIL ❌
  - **IDLE**: 
    - choose __next people__ who want to scan
    - if scan, that person is **HOLDING**
    - if __next people__ not scan & timeout = that person FAIL ❌



### Show Card ID
> any card
> useful for use that ID for assign something ex. Bomb card 
1. scan card
2. show card ID in hexadecimal (base 16)


[Website](https://anawina.github.io/hidden-bomb-card-game-by-arduino/)

[Poster Canva](https://www.canva.com/design/DAG2gupJsAI/sQAYxnCkkYAvnt-OidQYow/edit?utm_content=DAG2gupJsAI&utm_campaign=designshare&utm_medium=link2&utm_source=sharebutton)