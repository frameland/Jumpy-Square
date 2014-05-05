Strict
Import vsat
Import particles
Import extra


Class ParticleBackground
	
	Field emitter:ParticleEmitter
	Field lastFrame:Int
	Field baseUnit:Float
	
	Method New()
		baseUnit = Vsat.ScreenWidth2
		emitter = New ParticleEmitter
		emitter.InitWithSize(120)
		emitter.particleLifeSpan = 12
		emitter.particleLifeSpanVariance = 5.0
		emitter.emissionRate = 10
		
		emitter.size.Set(1, 1)
		emitter.endSize.Set(24, 24)
		If IsHD() Then emitter.endSize.Mul(2)

		emitter.startColor.Alpha = 0.1
		emitter.endColor.Alpha = 0.0
		
		emitter.speed = 85
		emitter.speedVariance = 35
		If IsHD()
			emitter.speed *= 2
			emitter.speedVariance *= 2
		End
		emitter.positionVariance.Set(baseUnit, Vsat.ScreenHeight2)
		
		SetNormal()
	End
	
	Method SetNormal:Void()
		emitter.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.8)
		emitter.emissionAngle = -90
		emitter.Start()
		emitter.FastForward(10, 0.016666)
	End
	
	Method SetPlay:Void()
		emitter.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.2)
		emitter.emissionAngle = 90
		emitter.Start()
		emitter.FastForward(10, 0.016666)
	End
	
	Method Update:Void(dt:Float)
		If lastFrame = Vsat.Frame
			Return
		End
		lastFrame = Vsat.Frame
		emitter.Update(dt)
	End
	
	Method Render:Void()
		emitter.Render()
	End
	
	
End




