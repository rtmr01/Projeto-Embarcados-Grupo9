import './style.css'
import mqtt from 'mqtt'
import Chart from 'chart.js/auto'

const BROKER_URL = 'ws://localhost:9001'
const TOPIC = 'esp32/trafego/dados'

const statusDot = document.getElementById('mqtt-status-dot')
const statusText = document.getElementById('mqtt-status-text')
const elPressao = document.getElementById('val-pressao')

// Configuração do Gráfico
const ctx = document.getElementById('pressureChart').getContext('2d')

// Gradiente para a linha
const gradient = ctx.createLinearGradient(0, 0, 0, 400);
gradient.addColorStop(0, 'rgba(59, 130, 246, 0.8)');   // --accent
gradient.addColorStop(1, 'rgba(59, 130, 246, 0.1)');

const MAX_DATA_POINTS = 50;
const chartConfig = {
  type: 'line',
  data: {
    labels: [],
    datasets: [{
      label: 'Pressão (V)',
      data: [],
      borderColor: '#3b82f6',
      backgroundColor: gradient,
      borderWidth: 3,
      pointRadius: 0,
      pointHoverRadius: 6,
      fill: true,
      tension: 0.4
    }]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    animation: {
      duration: 0 // Evita lag com muitas atualizações
    },
    scales: {
      y: {
        beginAtZero: false,
        grid: {
          color: 'rgba(255, 255, 255, 0.05)'
        },
        ticks: {
          color: '#94a3b8'
        }
      },
      x: {
        grid: {
          display: false
        },
        ticks: {
          color: '#94a3b8',
          maxTicksLimit: 10
        }
      }
    },
    plugins: {
      legend: {
        display: false
      },
      tooltip: {
        mode: 'index',
        intersect: false,
        backgroundColor: 'rgba(15, 23, 42, 0.9)',
        titleColor: '#f8fafc',
        bodyColor: '#3b82f6',
        borderColor: 'rgba(255, 255, 255, 0.1)',
        borderWidth: 1
      }
    },
    interaction: {
      mode: 'nearest',
      axis: 'x',
      intersect: false
    }
  }
}

const pressureChart = new Chart(ctx, chartConfig)

// Conexão MQTT
const client = mqtt.connect(BROKER_URL)

client.on('connect', () => {
  statusDot.classList.add('connected')
  statusText.innerText = 'Conectado ao Broker'
  client.subscribe(TOPIC, (err) => {
    if (!err) console.log('Inscrito no tópico: ' + TOPIC)
  })
})

client.on('error', (err) => {
  console.error('MQTT Error: ', err)
  statusDot.classList.remove('connected')
  statusText.innerText = 'Erro na conexão'
})

client.on('close', () => {
  statusDot.classList.remove('connected')
  statusText.innerText = 'Desconectado'
})

client.on('message', (topic, message) => {
  try {
    const data = JSON.parse(message.toString())
    
    if (data.pressaoV !== undefined) {
      // Atualiza o texto do canto superior direito
      elPressao.innerText = data.pressaoV.toFixed(3)
      
      // Atualiza o Gráfico
      const now = new Date()
      const timeStr = now.getHours().toString().padStart(2, '0') + ':' + 
                      now.getMinutes().toString().padStart(2, '0') + ':' + 
                      now.getSeconds().toString().padStart(2, '0')
      
      pressureChart.data.labels.push(timeStr)
      pressureChart.data.datasets[0].data.push(data.pressaoV)
      
      // Mantém apenas os últimos pontos no gráfico
      if (pressureChart.data.labels.length > MAX_DATA_POINTS) {
        pressureChart.data.labels.shift()
        pressureChart.data.datasets[0].data.shift()
      }
      
      pressureChart.update()
    }
  } catch (e) {
    console.error('Erro ao processar payload: ', e)
  }
})
